#include "stdafx.h"

#include "cm_commands_gui.hpp"

#include "cm_base64.hpp"
#include "cm_main.hpp"

#include "../network/core/http.h" //HttpCallback
#include "core/geometry_func.hpp" //maxdim
#include "settings_type.h"
#include "settings_func.h" //saveconfig
#include "3rdparty/md5/md5.h" //pass crypt
#include "network/network_func.h" //network chat
#include "strings_func.h"
#include "textbuf_gui.h"  //show query
#include "network/network.h" //networking
#include "ini_type.h" //ini file
#include "fileio_func.h" //personal dir
#include "error.h" //error message
#include "debug.h"
#include "window_func.h"
#include "window_gui.h"

#include <sstream>


namespace citymania {

bool _novahost = true;
IniFile *_inilogin = NULL;

static const int HTTPBUFLEN = 1024;
static const int MAX_COMMUNITY_STRING_LEN = 128;

// nova* stuff probabaly obsolete
static const char * const NOVAPOLIS_IPV4_PRIMARY   = "188.165.194.77";
static const char * const NOVAPOLIS_IPV6_PRIMARY   = "2a02:2b88:2:1::1d73:1";
static const char * const NOVAPOLIS_IPV4_SECONDARY = "89.111.65.225";
static const char * const NOVAPOLIS_IPV6_SECONDARY = "fe80::20e:7fff:fe23:bee0";
static const char * const NOVAPOLIS_STRING         = "CityMania";
static constexpr std::string_view NICE_HTTP_LOGIN          = "http://n-ice.org/openttd/gettoken_md5salt.php?user={}&password={}";
static constexpr std::string_view BTPRO_HTTP_LOGIN         = "http://openttd.btpro.nl/gettoken-enc.php?user={}&password={}";

static const char * const NOVA_IP_ADDRESSES[] = {
	NOVAPOLIS_IPV4_PRIMARY,
	NOVAPOLIS_IPV6_PRIMARY,
	NOVAPOLIS_IPV4_SECONDARY,
	NOVAPOLIS_IPV6_SECONDARY,
};

static const std::string CFG_LOGIN_FILE  = "citymania.cfg";
static const std::string CFG_LOGIN_KEY   = "login";
static const char * const NOVAPOLIS_LOGIN = "citymania_login";
static const char * const NOVAPOLIS_PW    = "citymania_pw";
static const char * const NICE_LOGIN      = "nice_login";
static const char * const NICE_PW         = "nice_pw";
static const char * const BTPRO_LOGIN     = "btpro_login";
static const char * const BTPRO_PW        = "btpro_pw";

static const char * const INI_LOGIN_KEYS[] = {
	NOVAPOLIS_LOGIN,
	NOVAPOLIS_PW,
	NICE_LOGIN,
	NICE_PW,
	BTPRO_LOGIN,
	BTPRO_PW,
};

/** Widget number of the commands window. */
enum CommandsToolbarWidgets {
	CTW_BACKGROUND,
	CTW_GOAL,
	CTW_SCORE,
	CTW_TOWN,
	CTW_QUEST,
	CTW_TOWN_ID,
	CTW_HINT,
	CTW_LOGIN,
	CTW_NAME,
	CTW_TIMELEFT,
	CTW_INFO,
	CTW_RESETME,
	CTW_SAVEME,
	CTW_HELP,
	CTW_RULES,
	CTW_CBHINT,
	CTW_CLIENTS,
	CTW_BEST,
	CTW_ME,
	CTW_RANK,
	CTW_TOPICS,
	CTW_TOPIC1,
	CTW_TOPIC2,
	CTW_TOPIC3,
	CTW_TOPIC4,
	CTW_TOPIC5,
	CTW_TOPIC6,
	CTW_CHOOSE_CARGO,
	CTW_CHOOSE_CARGO_AMOUNT,
	CTW_CHOOSE_CARGO_INCOME,
	CTW_NS0,
	CTW_NS1,
	CTW_NS2,
	CTW_NS3,
	CTW_NS4,
	CTW_NS5,
	CTW_NS6,
	CTW_NS7,
	CTW_NS8,
	CTW_NS9,
	CTW_NS10,
	CTW_NSX1,
	CTW_NSX2,
	CTW_NSX3,
	CTW_NSX4,
	CTW_NSX5,
	CTW_NSX6,
	CTW_NSX7,
	CTW_NSEND,
	CTW_CARGO_FIRST,
};

enum CommandsToolbarQuery {
	CTQ_TOWN_ID = 0,
	CTQ_LOGIN_NAME,
	CTQ_LOGIN_PASSWORD,
	CTQ_NAME_NEWNAME,
	CTQ_LOGIN_CREDENTIALS_PW,
};

enum CommandsToolbarCargoOption {
	CTW_OPTION_CARGO = 0,
	CTW_OPTION_CARGO_AMOUNT,
	CTW_OPTION_CARGO_INCOME,
};

enum LoginWindowWidgets {
	LWW_NOVAPOLIS,
	LWW_NICE,
	LWW_BTPRO,
	LWW_NOVAPOLIS_LOGIN,
	LWW_NOVAPOLIS_PW,
	LWW_NICE_LOGIN,
	LWW_NICE_PW,
	LWW_BTPRO_LOGIN,
	LWW_BTPRO_PW,
	LWW_USERNAME,
	LWW_PASSWORD,
};

enum LoginWindowQueryWidgets {
	LQW_NOVAPOLIS,
	LQW_NICE,
	LQW_BTPRO,
	LQW_NOVAPOLIS_LOGIN,
	LQW_NOVAPOLIS_PW,
	LQW_NICE_LOGIN,
	LQW_NICE_PW,
	LQW_BTPRO_LOGIN,
	LQW_BTPRO_PW,
};

enum CommunityName {
	CITYMANIA,
	NICE,
	BTPRO,
};

enum IniLoginKeys {
	NOVAPOLISUSER,
	NOVAPOLISPW,
	NICEUSER,
	NICEPW,
	BTPROUSER,
	BTPROPW,
};

char _inilogindata[6][MAX_COMMUNITY_STRING_LEN];

void AccountLogin(CommunityName community);
void IniReloadLogin();

bool novahost() {
	return _novahost;
}

// void strtomd5(char * buf, char * bufend, int length){
// 	MD5Hash digest;
// 	Md5 checksum;
// 	checksum.Append(buf, length);
// 	checksum.Finish(digest);
// 	md5sumToString(buf, bufend, digest);
// 	strtolower(buf);
// }

// void UrlEncode(char * buf, const char * buflast, const char * url){
// 	while(*url != '\0' && buf < buflast){
// 		if((*url >= '0' && *url <= '9') || (*url >= 'A' && *url <= 'Z') || (*url >= 'a' && *url <= 'z')
// 			|| *url == '-' || *url == '_' || *url == '.' || *url == '~'
// 		){
// 			*buf++ = *url++;
// 		}
// 		else{
// 			buf += seprintf(buf, buflast, "%%%02X", *url++);
// 		}
// 	}
// 	*buf = '\0';
// }


//ini login hadling
void IniLoginInitiate(){
	if(_inilogin != NULL) return; //it was already set
	_inilogin = new IniFile({CFG_LOGIN_KEY});
	_inilogin->LoadFromDisk(CFG_LOGIN_FILE, BASE_DIR);
	IniReloadLogin();
}

std::string GetLoginItem(const std::string& itemname){
	IniGroup &group = _inilogin->GetOrCreateGroup(CFG_LOGIN_KEY);
	IniItem &item = group.GetOrCreateItem(itemname);
	return item.value.value_or("");
}

void IniReloadLogin(){
	for(int i = 0, len = lengthof(INI_LOGIN_KEYS); i < len; i++){
		auto str = GetLoginItem(INI_LOGIN_KEYS[i]);
		if (str.empty()){
			str = GetString(CM_STR_LOGIN_WINDOW_NOT_SET);
		}
		strecpy(_inilogindata[i], str.c_str(), lastof(_inilogindata[i]));
	}
}

void SetLoginItem(const std::string& itemname, const std::string& value){
	IniGroup &group = _inilogin->GetOrCreateGroup(CFG_LOGIN_KEY);
	IniItem &item = group.GetOrCreateItem(itemname);
	item.SetValue(value);
	_inilogin->SaveToDisk(fmt::format("{}{}", _personal_dir, CFG_LOGIN_FILE));
	IniReloadLogin();
}

/** Commands toolbar window handler. */
/*
struct CommandsToolbarWindow : Window {

	CommandsToolbarQuery query_widget;
	CommandsToolbarCargoOption cargo_option;

	CommandsToolbarWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->cargo_option = CTW_OPTION_CARGO_AMOUNT;
		this->LowerWidget(CTW_CHOOSE_CARGO_AMOUNT);
	}

	virtual void SetStringParameters(int widget) const
	{
		if(widget >= CTW_NS0 && widget < CTW_NSEND){
			SetDParam(0, widget - CTW_NS0);
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		if (!_networking) return;
		char msg[64];
		switch (widget) {
			case CTW_GOAL:
				NetworkClientSendChatToServer("!goal");
				break;
			case CTW_SCORE:
				NetworkClientSendChatToServer("!score");
				break;
			case CTW_TOWN:
				NetworkClientSendChatToServer("!town");
				break;
			case CTW_QUEST:
				NetworkClientSendChatToServer("!quest");
				break;
			case CTW_TIMELEFT:
				NetworkClientSendChatToServer("!timeleft");
				break;
			case CTW_INFO:
				NetworkClientSendChatToServer("!info");
				break;
			case CTW_HINT:
				NetworkClientSendChatToServer("!hint");
				break;
			case CTW_RESETME:
				NetworkClientSendChatToServer("!resetme");
				break;
			case CTW_SAVEME:
				NetworkClientSendChatToServer("!saveme");
				break;
			case CTW_HELP:
				NetworkClientSendChatToServer("!help");
				break;
			case CTW_RULES:
				NetworkClientSendChatToServer("!rules");
				break;
			case CTW_CBHINT:
				NetworkClientSendChatToServer("!hint");
				break;
			case CTW_CLIENTS:
				NetworkClientSendChatToServer("!clients");
				break;
			case CTW_BEST:
				NetworkClientSendChatToServer("!best");
				break;
			case CTW_ME:
				NetworkClientSendChatToServer("!me");
				break;
			case CTW_RANK:
				NetworkClientSendChatToServer("!rank");
				break;
			case CTW_TOPICS:
				NetworkClientSendChatToServer("!topic");
				break;
			case CTW_TOWN_ID:
				this->query_widget = CTQ_TOWN_ID;
				ShowQueryString(STR_EMPTY, STR_TOOLBAR_COMMANDS_TOWN_QUERY, 8, this, CS_NUMERAL, QSF_NONE);
				break;
			case CTW_LOGIN:
				this->query_widget = CTQ_LOGIN_NAME;
				ShowQueryString(STR_EMPTY, STR_TOOLBAR_COMMANDS_LOGIN_NAME_QUERY, 24, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case CTW_NAME:
				this->query_widget = CTQ_NAME_NEWNAME;
				ShowQueryString(STR_EMPTY, STR_TOOLBAR_COMMANDS_NAME_NEWNAME_QUERY, 25, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case CTW_TOPIC1:
			case CTW_TOPIC2:
			case CTW_TOPIC3:
			case CTW_TOPIC4:
			case CTW_TOPIC5:
			case CTW_TOPIC6:
				seprintf(msg, lastof(msg), "!topic %i", widget - CTW_TOPIC1 + 1);
				NetworkClientSendChatToServer(msg);
				break;
			case CTW_CHOOSE_CARGO:
			case CTW_CHOOSE_CARGO_AMOUNT:
			case CTW_CHOOSE_CARGO_INCOME:
				this->cargo_option = (CommandsToolbarCargoOption)(widget - CTW_CHOOSE_CARGO);
				this->RaiseWidget(CTW_CHOOSE_CARGO);
				this->RaiseWidget(CTW_CHOOSE_CARGO_AMOUNT);
				this->RaiseWidget(CTW_CHOOSE_CARGO_INCOME);
				this->LowerWidget(widget);
				this->SetWidgetDirty(CTW_CHOOSE_CARGO);
				this->SetWidgetDirty(CTW_CHOOSE_CARGO_AMOUNT);
				this->SetWidgetDirty(CTW_CHOOSE_CARGO_INCOME);
				break;
			default:
				if(widget >= CTW_NS0 && widget < CTW_NSEND){
					char ip[16];
					// if(widget < CTW_NSX4)
					strecpy(ip, NOVAPOLIS_IPV4_PRIMARY, lastof(ip));
					// else strecpy(ip, NOVAPOLIS_IPV4_SECONDARY, lastof(ip));

					// FIXME
					// NetworkClientConnectGame(ip, (3980 + widget - CTW_NS0), COMPANY_SPECTATOR);
				}
				else if (widget >= CTW_CARGO_FIRST) {
					int i = widget - CTW_CARGO_FIRST;
					char name[128];
					GetString(name, _sorted_cargo_specs[i]->name, lastof(name));
					switch(this->cargo_option){
						case CTW_OPTION_CARGO:        seprintf(msg, lastof(msg), "!%s", name); break;
						case CTW_OPTION_CARGO_AMOUNT: seprintf(msg, lastof(msg), "!A%s", name); break;
						case CTW_OPTION_CARGO_INCOME: seprintf(msg, lastof(msg), "!I%s", name); break;
					}
					NetworkClientSendChatToServer(msg);
					this->ToggleWidgetLoweredState(widget);
					this->SetWidgetDirty(widget);
				}
				break;
		}
	}

	void OnQueryTextFinished(char * str)
	{
		if (!_networking || str == NULL) return;
		char msg[128];
		switch (this->query_widget) {
			case CTQ_TOWN_ID:
				seprintf(msg, lastof(msg), "!town %s", str);
				NetworkClientSendChatToServer(msg);
				break;
			case CTQ_LOGIN_NAME:
				seprintf(msg, lastof(msg), "!login %s", str);
				NetworkClientSendChatToServer(msg);
				break;
			case CTQ_NAME_NEWNAME:
				seprintf(msg, lastof(msg), "!name %s", str);
				NetworkClientSendChatToServer(msg);
				break;
			default:
				NOT_REACHED();
		}
	}

	void UpdateWidgetSize(int widget, Dimension *size, const Dimension &padding, Dimension *fill, Dimension *resize)
	{
		if (widget < CTW_CARGO_FIRST) return;

		const CargoSpec *cs = _sorted_cargo_specs[widget - CTW_CARGO_FIRST];
		SetDParam(0, cs->name);
		Dimension d = GetStringBoundingBox(STR_GRAPH_CARGO_PAYMENT_CARGO);
		d.width += 14; // colour field
		d.width += WidgetDimensions::scaled.framerect.Horizontal();
		d.height += WidgetDimensions::scaled.framerect.Vertical();
		*size = maxdim(d, *size);
	}

	void DrawWidget(const Rect &r, int widget) const
	{
		if (widget < CTW_CARGO_FIRST) return;

		const CargoSpec *cs = _sorted_cargo_specs[widget - CTW_CARGO_FIRST];
		bool rtl = _current_text_dir == TD_RTL;

		/ * Since the buttons have no text, no images,
		 * both the text and the coloured box have to be manually painted.
		 * clk_dif will move one pixel down and one pixel to the right
		 * when the button is clicked */ /*
		byte clk_dif = this->IsWidgetLowered(widget) ? 1 : 0;
		int x = r.left + WidgetDimensions::scaled.framerect.left;
		int y = r.top;

		int rect_x = clk_dif + (rtl ? r.right - 12 : r.left + WidgetDimensions::scaled.framerect.left);

		GfxFillRect(rect_x, y + clk_dif, rect_x + 8, y + 5 + clk_dif, 0);
		GfxFillRect(rect_x + 1, y + 1 + clk_dif, rect_x + 7, y + 4 + clk_dif, cs->legend_colour);
		SetDParam(0, cs->name);
		DrawString(rtl ? r.left : x + 14 + clk_dif, (rtl ? r.right - 14 + clk_dif : r.right), y + clk_dif, STR_GRAPH_CARGO_PAYMENT_CARGO);
	}

	void OnHundredthTick()
	{
		for (int i = 0; i < _sorted_standard_cargo_specs.size(); i++) {
			if (this->IsWidgetLowered(i + CTW_CARGO_FIRST)) {
				static int x = 0;
				x++;
				if (x >= 2) {
					this->ToggleWidgetLoweredState(i + CTW_CARGO_FIRST);
					this->SetWidgetDirty(i + CTW_CARGO_FIRST);
					x = 0;
				}
			}
		}
	}

};

/ ** Construct the row containing the digit keys. *//*
static NWidgetBase *MakeCargoButtons(int *biggest_index)
{
	NWidgetVertical *ver = new NWidgetVertical;

	for (int i = 0; i < _sorted_standard_cargo_specs.size(); i++) {
		NWidgetBackground *leaf = new NWidgetBackground(WWT_PANEL, COLOUR_ORANGE, CTW_CARGO_FIRST + i, NULL);
		leaf->tool_tip = STR_GRAPH_CARGO_PAYMENT_TOGGLE_CARGO;
		leaf->SetFill(1, 0);
		ver->Add(leaf);
	}
	*biggest_index = CTW_CARGO_FIRST + _sorted_standard_cargo_specs.size() - 1;
	return ver;
}

static const NWidgetPart _nested_commands_toolbar_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_TOOLBAR_COMMANDS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_GOAL), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_GOAL_CAPTION, STR_TOOLBAR_COMMANDS_GOAL_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_QUEST), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_QUEST_CAPTION, STR_TOOLBAR_COMMANDS_QUEST_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_SCORE), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_SCORE_CAPTION, STR_TOOLBAR_COMMANDS_SCORE_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOWN), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOWN_CAPTION, STR_TOOLBAR_COMMANDS_TOWN_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOWN_ID), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOWN_ID_CAPTION, STR_TOOLBAR_COMMANDS_TOWN_ID_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_HINT), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_HINT_CAPTION, STR_TOOLBAR_COMMANDS_HINT_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_LOGIN), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_LOGIN_CAPTION, STR_TOOLBAR_COMMANDS_LOGIN_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_NAME), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NAME_CAPTION, STR_TOOLBAR_COMMANDS_NAME_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TIMELEFT), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TIMELEFT_CAPTION, STR_TOOLBAR_COMMANDS_TIMELEFT_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_INFO), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_INFO_CAPTION, STR_TOOLBAR_COMMANDS_INFO_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS0), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS1), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS2), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS3), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS4), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS5), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS6), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS7), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS9), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS10), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX1), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX2), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX3), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX4), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX5), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_RESETME), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_RESETME_CAPTION, STR_TOOLBAR_COMMANDS_RESETME_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_SAVEME), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_SAVEME_CAPTION, STR_TOOLBAR_COMMANDS_SAVEME_TOOLTIP),
			EndContainer(),
			//more
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPICS), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPICS_CAPTION, STR_TOOLBAR_COMMANDS_TOPICS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_HELP), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_HELP_CAPTION, STR_TOOLBAR_COMMANDS_HELP_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC1), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC1_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC1_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_RULES), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_RULES_CAPTION, STR_TOOLBAR_COMMANDS_RULES_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC2), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC2_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC2_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_CBHINT), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_CBHINT_CAPTION, STR_TOOLBAR_COMMANDS_CBHINT_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC3), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC3_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC3_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_BEST), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_BEST_CAPTION, STR_TOOLBAR_COMMANDS_BEST_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC4), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC4_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC4_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_RANK), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_RANK_CAPTION, STR_TOOLBAR_COMMANDS_RANK_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC5), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC5_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC5_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_ME), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_ME_CAPTION, STR_TOOLBAR_COMMANDS_ME_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC6), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC6_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC6_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(0, 1), EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(0, 1), EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, CTW_CHOOSE_CARGO_AMOUNT), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_OPTION_CARGO_A_CAPTION, STR_TOOLBAR_COMMANDS_OPTION_CARGO_A_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, CTW_CHOOSE_CARGO_INCOME), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_OPTION_CARGO_I_CAPTION, STR_TOOLBAR_COMMANDS_OPTION_CARGO_I_TOOLTIP),
			NWidget(WWT_TEXTBTN, COLOUR_GREY, CTW_CHOOSE_CARGO), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_OPTION_CARGO_CAPTION, STR_TOOLBAR_COMMANDS_OPTION_CARGO_TOOLTIP),
			NWidgetFunction(MakeCargoButtons),
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(0, 1), EndContainer(),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _commands_toolbar_desc(
	WDP_ALIGN_TOOLBAR, NULL, 0, 0,
	WC_COMMAND_TOOLBAR, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_commands_toolbar_widgets, lengthof(_nested_commands_toolbar_widgets)
);*/

void ShowCommandsToolbar()
{
	// DeleteWindowByClass(WC_COMMAND_TOOLBAR);
	// AllocateWindowDescFront<CommandsToolbarWindow>(&_commands_toolbar_desc, 0);
}

// login window
class GetHTTPContent: public HTTPCallback {
public:
	GetHTTPContent(const std::string &uri): uri{uri} {
		this->proccessing = false;
		this->buf_last = lastof(buf);
	}
	bool proccessing;

	void InitiateLoginSequence() {
		if(this->proccessing) return;
		this->proccessing = true;
		this->cursor = this->buf;
		NetworkHTTPSocketHandler::Connect(this->uri, this);
	}

	void OnReceiveData(std::unique_ptr<char[]> data, size_t length) override {
		if (data.get() == nullptr) {
			this->cursor = nullptr;
			this->LoginAlready();
		} else {
			for(size_t i = 0; i < length && this->cursor < this->buf_last; i++, this->cursor++) {
				*(this->cursor) = data.get()[i];
			}
			*(this->cursor) = '\0';
		}
	}

	void OnFailure() override {
		ShowErrorMessage(CM_STR_LOGIN_ERROR_SIGN_IN_FAILED, INVALID_STRING_ID, WL_ERROR);
	}

	bool IsCancelled() const override {
		return false;
	}

	void LoginAlready(){
		if(strlen(this->buf) == 4 && _networking){
			NetworkClientSendChatToServer(fmt::format("!login {}", this->buf));
		} else{
			ShowErrorMessage(CM_STR_LOGIN_ERROR_BAD_INPUT, INVALID_STRING_ID, WL_ERROR);
		}
		this->proccessing = false;
	}

	virtual ~GetHTTPContent() {
	}
private:
	char buf[HTTPBUFLEN];
	char *buf_last;
	char *cursor;
	std::string uri;
};

std::string urlencode(const std::string &s) {
    static const char lookup[]= "0123456789abcdef";
    std::stringstream e;
    for(int i=0, ix=s.length(); i<ix; i++)
    {
        const char& c = s[i];
        if ( (48 <= c && c <= 57) ||//0-9
             (65 <= c && c <= 90) ||//abc...xyz
             (97 <= c && c <= 122) || //ABC...XYZ
             (c=='-' || c=='_' || c=='.' || c=='~')
        )
        {
            e << c;
        }
        else
        {
            e << '%';
            e << lookup[ (c&0xF0)>>4 ];
            e << lookup[ (c&0x0F) ];
        }
    }
    return e.str();
}

std::string btpro_encode(const char *value) {
	return urlencode(base64_encode((const unsigned char *)value, strlen(value)));
}

//send login
void AccountLogin(CommunityName community){
	std::string uri;
	switch(community){
		case CITYMANIA:
			NetworkClientSendChatToServer(fmt::format("!login {} {}", _inilogindata[NOVAPOLISUSER], _inilogindata[NOVAPOLISPW]));
			return;
		case NICE:
			uri = fmt::format(NICE_HTTP_LOGIN, GetLoginItem(NICE_LOGIN), GetLoginItem(NICE_PW));
			break;
		case BTPRO: {
			uri = fmt::format(BTPRO_HTTP_LOGIN,
			         btpro_encode(GetLoginItem(BTPRO_LOGIN).c_str()),
			         btpro_encode(GetLoginItem(BTPRO_PW).c_str()));
			break;
		}
		default:
			return;
	}
	static GetHTTPContent login(uri);
	login.InitiateLoginSequence();
}

//login window
struct LoginWindow : Window {
	LoginWindowQueryWidgets query_widget;

	LoginWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		// if(_novahost || !_networking){
		// 	this->DisableWidget(LWW_NICE);
		// 	this->DisableWidget(LWW_BTPRO);
		// }
		// if(!_novahost || !_networking) this->DisableWidget(LWW_NOVAPOLIS);
	}

	void SetStringParameters(int widget) const override
	{
		switch(widget){
			case LWW_NOVAPOLIS_LOGIN:
				SetDParamStr(0, _inilogindata[NOVAPOLISUSER]);
				break;
			case LWW_NOVAPOLIS_PW:
				SetDParam(0, (GetLoginItem(NOVAPOLIS_PW).empty() ? CM_STR_LOGIN_WINDOW_NOT_SET : CM_STR_LOGIN_WINDOW_SET));
				break;
			case LWW_NICE_LOGIN:
				SetDParamStr(0, _inilogindata[NICEUSER]);
				break;
			case LWW_NICE_PW:
				SetDParam(0, (GetLoginItem(NICE_PW).empty() ? CM_STR_LOGIN_WINDOW_NOT_SET : CM_STR_LOGIN_WINDOW_SET));
				break;
			case LWW_BTPRO_LOGIN:
				SetDParamStr(0, _inilogindata[BTPROUSER]);
				break;
			case LWW_BTPRO_PW:
				SetDParam(0, (GetLoginItem(BTPRO_PW).empty() ? CM_STR_LOGIN_WINDOW_NOT_SET : CM_STR_LOGIN_WINDOW_SET));
				break;
		}
	}

	void OnClick([[maybe_unused]] Point pt, int widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case LWW_NOVAPOLIS:
				if(/*_novahost && */_networking) AccountLogin(CITYMANIA);
				break;
			case LWW_NICE:
				if(_networking) AccountLogin(NICE);
				break;
			case LWW_BTPRO:
				if(_networking) AccountLogin(BTPRO);
				break;
			case LWW_NOVAPOLIS_LOGIN:
			case LWW_NICE_LOGIN:
			case LWW_BTPRO_LOGIN:
				this->query_widget = (LoginWindowQueryWidgets)widget;
				ShowQueryString(STR_EMPTY, CM_STR_LOGIN_WINDOW_CHANGE_USERNAME, MAX_COMMUNITY_STRING_LEN, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case LWW_NOVAPOLIS_PW:
			case LWW_NICE_PW:
			case LWW_BTPRO_PW:
				this->query_widget = (LoginWindowQueryWidgets)widget;
				ShowQueryString(STR_EMPTY, CM_STR_LOGIN_WINDOW_CHANGE_PASSWORD, MAX_COMMUNITY_STRING_LEN, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
		}
	}

	void OnQueryTextFinished(char * str) override
	{
		if (str == NULL) return;
		switch(this->query_widget){
			case LQW_NOVAPOLIS_LOGIN: {
				SetLoginItem(NOVAPOLIS_LOGIN, str);
				break;
			}
			case LQW_NOVAPOLIS_PW: {
				SetLoginItem(NOVAPOLIS_PW, str);
				break;
			}
			case LQW_NICE_LOGIN:
			case LQW_BTPRO_LOGIN:
			case LQW_BTPRO_PW: {
				auto item = urlencode(str);
				SetLoginItem(INI_LOGIN_KEYS[this->query_widget - 3], item); // - LWW_NICE_LOGIN + NICE_LOGIN
				break;
			}
			case LQW_NICE_PW: {
				Md5 password, salted_password;
				MD5Hash digest;

				password.Append(str, strlen(str));
				password.Finish(digest);
				auto first_pass = fmt::format("{:02x}", fmt::join(digest, ""));

				auto tobe_salted = fmt::format("nice{}client", first_pass);
				salted_password.Append(tobe_salted.c_str(),tobe_salted.length());
				salted_password.Finish(digest);
				auto second_pass = fmt::format("{:02x}", fmt::join(digest, ""));

				// Save the result to citymania.cfg
				SetLoginItem(NICE_PW, second_pass);
				break;
			}
			default: return;
		}
		this->SetDirty();
	}
};

static const NWidgetPart _nested_login_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(CM_STR_LOGIN_WINDOW_CAPTION, 0),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0),
		NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPadding(10),
			//novapolis
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_USERNAME), SetDataTip(CM_STR_LOGIN_WINDOW_USERNAME, 0),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_PASSWORD), SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD, 0),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_NOVAPOLIS_LOGIN), SetMinimalSize(60, 20), SetFill(1, 0), SetDataTip(CM_STR_LOGIN_WINDOW_USERNAME_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_USERNAME_HELPTEXT),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_NOVAPOLIS_PW), SetMinimalSize(30, 20), SetFill(1, 0), SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_PASSWORD_HELPTEXT),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			NWidget(WWT_PUSHTXTBTN, COLOUR_PURPLE, LWW_NOVAPOLIS), SetMinimalSize(100, 30), SetFill(1, 0), SetDataTip(CM_STR_LOGIN_WINDOW_CITYMANIA, CM_STR_LOGIN_WINDOW_SIGN_IN_HELPTEXT),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			//n-ice
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_USERNAME), SetDataTip(CM_STR_LOGIN_WINDOW_USERNAME, 0),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_PASSWORD), SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD, 0),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_NICE_LOGIN), SetMinimalSize(60, 20), SetFill(1, 0), SetDataTip(CM_STR_LOGIN_WINDOW_USERNAME_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_USERNAME_HELPTEXT),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_NICE_PW), SetMinimalSize(30, 20), SetFill(1, 0), SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_PASSWORD_HELPTEXT),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, LWW_NICE), SetMinimalSize(100, 30), SetFill(1, 0), SetDataTip(CM_STR_LOGIN_WINDOW_NICE, CM_STR_LOGIN_WINDOW_SIGN_IN_HELPTEXT),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			//btpro
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_USERNAME), SetDataTip(CM_STR_LOGIN_WINDOW_USERNAME, 0),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_PASSWORD), SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD, 0),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_BTPRO_LOGIN), SetMinimalSize(60, 20), SetFill(1, 0), SetDataTip(CM_STR_LOGIN_WINDOW_USERNAME_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_USERNAME_HELPTEXT),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_BTPRO_PW), SetMinimalSize(30, 20), SetFill(1, 0), SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_PASSWORD_HELPTEXT),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, LWW_BTPRO), SetMinimalSize(100, 30), SetFill(1, 0), SetDataTip(CM_STR_LOGIN_WINDOW_BTPRO, CM_STR_LOGIN_WINDOW_SIGN_IN_HELPTEXT),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _login_window_desc(__FILE__, __LINE__,
	WDP_CENTER, "cm_login", 0, 0,
	CM_WC_LOGIN_WINDOW, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_login_window_widgets), std::end(_nested_login_window_widgets)
);

void ShowLoginWindow()
{
	IniLoginInitiate();
	CloseWindowByClass(CM_WC_LOGIN_WINDOW);
	AllocateWindowDescFront<LoginWindow>(&_login_window_desc, 0);
}

} // namespace citymania
