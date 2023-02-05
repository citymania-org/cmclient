import glob
import re
from pathlib import Path
from pprint import pprint

RX_COMMAND = re.compile(r'(?P<returns>CommandCost|std::tuple<CommandCost, [^>]*>) (?P<name>Cmd\w*)\((?P<args>[^)]*)\);')
RX_DEF_TRAIT = re.compile(r'DEF_CMD_TRAIT\((?P<constant>\w+),\s+(?P<function>\w+),\s+[^,]*,\s+(?P<category>\w+)\)')
RX_ARG = re.compile(r'(?P<type>(:?const |)[\w:]* &?)(?P<name>\w*)')
RX_CALLBACK = re.compile(r'void\s+(?P<name>Cc\w+)\(Commands')
RX_CALLBACK_REF = re.compile(r'CommandCallback\s+(?P<name>Cc\w+);')
RX_CAMEL_TO_SNAKE = re.compile(r'(?<!^)(?=[A-Z])')

FILES = [
    'src/misc_cmd.h',
    'src/object_cmd.h',
    'src/order_cmd.h',
    'src/rail_cmd.h',
    'src/road_cmd.h',
    'src/station_cmd.h',
    'src/town_cmd.h',
    'src/tunnelbridge_cmd.h',
]

BASE_DIR = Path(__file__).parent
OUTPUT = BASE_DIR / 'src/citymania/generated/cm_gen_commands'


def parse_commands():
    res = []
    includes = []
    callbacks = []
    for f in glob.glob(str(BASE_DIR / 'src' / '*_cmd.h')):
        includes.append(Path(f).name)
        data = open(f).read()
        traits = {}
        for constant, name, category in RX_DEF_TRAIT.findall(data):
            traits[name] = constant, category
        callbacks.extend(RX_CALLBACK.findall(data))
        callbacks.extend(RX_CALLBACK_REF.findall(data))
        for returns, name, args_str in RX_COMMAND.findall(data):
            trait = traits.get(name)
            if not trait:
                print(f'Not a command: {name}')
                continue
            print(f, name, end=' ', flush=True)
            constant, category = trait
            if returns.startswith('std::tuple'):
                ret_type = returns[24: -1]
            else:
                ret_type = None
            args = [RX_ARG.fullmatch(x).group('type', 'name') for x in args_str.split(', ')]
            args = args[1:]  # flags
            first_tile_arg = (args[0][0].strip() == 'TileIndex')
            if first_tile_arg:
                args = args[1:]
            print(constant, category, args)
            res.append({
                'name': name[3:],
                'constant': constant,
                'category': category,
                'args': args,
                'first_tile_arg': first_tile_arg,
                'returns': ret_type,
            })
    return res, includes, callbacks

CPP_TEMPLATES = '''\
inline constexpr size_t _callback_tuple_size = std::tuple_size_v<decltype(_callback_tuple)>;

#ifdef SILENCE_GCC_FUNCTION_POINTER_CAST
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

template <size_t... i>
inline auto MakeCallbackTable(std::index_sequence<i...>) noexcept {
    return std::array<::CommandCallback *, sizeof...(i)>{{ reinterpret_cast<::CommandCallback *>(reinterpret_cast<void(*)()>(std::get<i>(_callback_tuple)))... }}; // MingW64 fails linking when casting a pointer to its own type. To work around, cast it to some other type first.
}
/** Type-erased table of callbacks. */
static auto _callback_table = MakeCallbackTable(std::make_index_sequence<_callback_tuple_size>{});\n
template <typename T> struct CallbackArgsHelper;
template <typename... Targs>
struct CallbackArgsHelper<void(*const)(Commands, const CommandCost &, Targs...)> {
    using Args = std::tuple<std::decay_t<Targs>...>;
};
#ifdef SILENCE_GCC_FUNCTION_POINTER_CAST
#   pragma GCC diagnostic pop
#endif

static size_t FindCallbackIndex(::CommandCallback *callback) {
    if (auto it = std::find(std::cbegin(_callback_table), std::cend(_callback_table), callback); it != std::cend(_callback_table)) {
        return static_cast<size_t>(std::distance(std::cbegin(_callback_table), it));
    }
    return std::numeric_limits<size_t>::max();
}

template <Commands Tcmd, size_t Tcb, typename... Targs>
bool _DoPost(StringID err_msg, TileIndex tile, Targs... args) {
    return ::Command<Tcmd>::Post(err_msg, std::get<Tcb>(_callback_tuple), tile, std::forward<Targs>(args)...);
}
template <Commands Tcmd, size_t Tcb, typename... Targs>
constexpr auto MakeCallback() noexcept {
    /* Check if the callback matches with the command arguments. If not, don''t generate an Unpack proc. */
    using Tcallback = std::tuple_element_t<Tcb, decltype(_callback_tuple)>;
    if constexpr (std::is_same_v<Tcallback, ::CommandCallback * const> ||
            std::is_same_v<Tcallback, CommandCallbackData * const> ||
            std::is_same_v<typename CommandTraits<Tcmd>::CbArgs, typename CallbackArgsHelper<Tcallback>::Args> ||
            (!std::is_void_v<typename CommandTraits<Tcmd>::RetTypes> && std::is_same_v<typename CallbackArgsHelper<typename CommandTraits<Tcmd>::RetCallbackProc const>::Args, typename CallbackArgsHelper<Tcallback>::Args>)) {
        return &_DoPost<Tcmd, Tcb, Targs...>;
    } else {
        return nullptr;
    }
}

template <Commands Tcmd, typename... Targs, size_t... i>
inline constexpr auto MakeDispatchTableHelper(std::index_sequence<i...>) noexcept
{
    return std::array<bool (*)(StringID err_msg, TileIndex tile, Targs...), sizeof...(i)>{MakeCallback<Tcmd, i, Targs...>()... };
}

template <Commands Tcmd, typename... Targs>
inline constexpr auto MakeDispatchTable() noexcept
{
    return MakeDispatchTableHelper<Tcmd, Targs...>(std::make_index_sequence<_callback_tuple_size>{});
}

'''

def run():
    commands, includes, callbacks = parse_commands()
    with open(OUTPUT.with_suffix('.hpp'), 'w') as f:
        f.write(
            '// This file is generated by gen_commands.py, do not edit\n\n'
            '#ifndef CM_GEN_COMMANDS_HPP\n'
            '#define CM_GEN_COMMANDS_HPP\n'
            '#include "../cm_command_type.hpp"\n'
        )
        for i in includes:
            f.write(f'#include "../../{i}"\n')
        f.write('\n')
        f.write(
            'namespace citymania {\n'
            'namespace cmd {\n\n'
        )
        for cmd in commands:
            name = cmd['name']
            args_list = ', '.join(f'{at}{an}' for at, an in cmd['args'])
            args_init = ', '.join(f'{an}{{{an}}}' for _, an in cmd['args'])
            f.write(
                f'class {name}: public Command {{\n'
                f'public:\n'
            )
            for at, an in cmd['args']:
                f.write(f'    {at}{an};\n')
            f.write(f'\n')
            if args_init:
                f.write(
                    f'    {name}({args_list})\n'
                    f'        :{args_init} {{}}\n'
                )
            else:
                f.write(f'    {name}({args_list}) {{}}\n')

            if cmd.get('first_tile_arg'):
                separator = ', ' if args_list else ''
                f.write(
                    f'    {name}(TileIndex tile{separator}{args_list})\n'
                    f'        :Command{{tile}}{separator}{args_init} {{}}\n'
                )
            f.write(
                f'    ~{name}() override {{}}\n'
                f'\n'
                f'    bool _post(::CommandCallback * callback) override;\n'
                f'    CommandCost _do(DoCommandFlag flags) override;\n'
                f'    Commands get_command() override;\n'
                f'}};\n\n'
            )
        f.write(
            '}  // namespace cmd\n'
            '}  // namespace citymania\n'
            '#endif\n'
        )

    with open(OUTPUT.with_suffix('.cpp'), 'w') as f:
        f.write(
            '// This file is generated by gen_commands.py, do not edit\n\n'
            '#include "../../stdafx.h"\n'
            '#include "cm_gen_commands.hpp"\n'
        )
        for fn in FILES:
            f.write(f'#include "../../{fn}"\n')
        f.write(
            'namespace citymania {\n'
            'namespace cmd {\n\n'
        )
        f.write(
            '/*\n'
            ' * The code is mostly copied from network_command.cpp\n'
            ' * but the table is not the same.\n'
            ' */\n'
            'static constexpr auto _callback_tuple = std::make_tuple(\n'
            '    (::CommandCallback *)nullptr, // Make sure this is actually a pointer-to-function.\n'
        )
        for i, cb in enumerate(callbacks):
            comma = ',' if i != len(callbacks) - 1 else ''
            f.write(f'    &{cb}{comma}\n')
        f.write(');\n\n')
        f.write(CPP_TEMPLATES)

        for cmd in commands:
            name = cmd['name']
            constant = cmd['constant']
            this_args_list = ', '.join(f'this->{an}' for _, an in cmd['args'])
            args_list = ', '.join(f'{an}' for _, an in cmd['args'])
            args_type_list = ', '.join(f'{at}' for at, an in cmd['args'])
            test_args_list = args_list
            if cmd.get('first_tile_arg'):
                if args_list:
                    test_args_list = f'this->tile, ' + args_list
                else:
                    test_args_list = f'this->tile'

            cost_getter = '' if cmd['returns'] is None else 'std::get<0>'
            sep_args_list = sep_args_type_list = sep_this_args_list = ''
            if args_list:
                sep_args_list = ', ' + args_list
                sep_args_type_list = ', ' + args_type_list
                sep_this_args_list = ', ' + this_args_list
            f.write(
                f'Commands {name}::get_command() {{ return {constant}; }}\n'
                f'static constexpr auto _{name}_dispatch = MakeDispatchTable<{constant}{sep_args_type_list}>();\n'
                f'bool {name}::_post(::CommandCallback *callback) {{\n'
                f'    return _{name}_dispatch[FindCallbackIndex(callback)](this->error, this->tile{sep_this_args_list});\n'
                '}\n'
                f'CommandCost {name}::_do(DoCommandFlag flags) {{\n'
                f'    return {cost_getter}(::Command<{constant}>::Do(flags, {test_args_list}));\n'
                '}\n'
            )
            f.write('\n')
        f.write(
            '}  // namespace cmd\n'
            '}  // namespace citymania\n'
        )

if __name__ == "__main__":
    run()
