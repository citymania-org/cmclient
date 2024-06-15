#include "stdafx.h"

#include "cm_commands_gui.hpp"

#include "cm_base64.hpp"
#include "cm_main.hpp"

#include "../network/core/http.h" //HttpCallback
#include "../network/network_client.h" //for servername
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

/* admin company buttons */
bool _admin = false;
int ACB_left = 0;
int ACB_top = 0;
int ACB_width = 0;
int ACB_Location[3][15];

/* server buttons */
std::string _server_list_text; // list from http
std::string _cc_address;  // current adddress
std::string _cc_name;     // current name
int _cc_porti;            // current port
std::int8_t _fromlast = 0;
int _left, _top, _height; 
int ls_left, ls_top, ls_height; // for last server
int _servercount;  // how many servers

static const int HTTPBUFLEN = 1024;
static const int MAX_COMMUNITY_STRING_LEN = 128;

static constexpr std::string_view NICE_HTTP_LOGIN          = "http://n-ice.org/openttd/gettoken_md5salt.php?user={}&password={}";
static constexpr std::string_view BTPRO_HTTP_LOGIN         = "http://openttd.btpro.nl/gettoken-enc.php?user={}&password={}";

static const std::string CFG_FILE  = "citymania.cfg";
static const std::string CFG_LOGIN_KEY   = "login";
static const std::string CFG_SERVER_KEY =  "server";
static const char * const NOVAPOLIS_LOGIN = "citymania_login";
static const char * const NOVAPOLIS_PW    = "citymania_pw";
static const char * const NICE_LOGIN      = "nice_login";
static const char * const NICE_PW         = "nice_pw";
static const char * const BTPRO_LOGIN     = "btpro_login";
static const char * const BTPRO_PW        = "btpro_pw";
static const char * const NICE_ADMIN_PW   = "nice_admin_pw";
static const char * const BTPRO_ADMIN_PW  = "btpro_admin_pw";
static const char * const ADMIN           = "admin";

static const char * const COMMUNITY       = "community";

static const char * const INI_LOGIN_KEYS[] = {
	NOVAPOLIS_LOGIN,
	NOVAPOLIS_PW,
	NICE_LOGIN,
	NICE_PW,
	BTPRO_LOGIN,
	BTPRO_PW,
	NICE_ADMIN_PW,
	BTPRO_ADMIN_PW,
	ADMIN,
};


static const char *const INI_SERVER_KEYS[] = {
	COMMUNITY,
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
	LWW_USER_NAME,
	LWW_USER_PW,
	LWW_ADMIN_PW,
	LWW_USER_LOGIN,
	LWW_USER_LOGOUT,
	LWW_ADMIN_LOGIN,
	LWW_ADMIN_LOGOUT,
	LWW_COMMUNITY,
    LWW_USERNAME,
    LWW_PASSWORD,
};

enum LoginWindowQueryWidgets {
	LQW_USER_NAME,
	LQW_USER_PW,
	LQW_ADMIN_PW
};

enum AdminCompanyButtonsWidgets {
	ACB_COMPANY_EMPTY,
    ACB_COMPANY_LOCK,
    ACB_COMPANY_UNLOCK,
	ACB_COMPANY_NEWSTICKET,
	ACB_COMPANY_NEWSTICKET_COMP,
	ACB_COMPANY_RESET,
	ACB_COMPANY_RESET_SPEC,
	ACB_COMPANY_RESET_KICK,
	ACB_COMPANY_KNOWN,
	ACB_COMPANY_RESET_KNOWN,
	ACB_COMPANY_MOVE_PLAYER,
	ACB_RESET_COMPANY_TIMER_120,
	ACB_RESET_COMPANY_TIMER,
	ACB_RESET_COMPANY_TIMER_CANCEL,
	ACB_COMPANY_SUSPEND,
	ACB_COMPANY_UNSUSPEND,
	ACB_COMPANY_AWARNING,
	ACB_COMPANY_JOIN2,
	ACB_COMPANY_LEAVE,
	ACB_COMPANY_CANCEL,
	ACB_COMPANY_CAPTION,
};

enum AdminCompanyButtonsQueryWidgets {
	ACBQ_RESET_COMPANY_TIMER,
	ACBQ_COMPANY_NEWSTICKET,
	ACBQ_COMPANY_NEWSTICKET_COMP,
	ACBQ_COMPANY_MOVE_PLAYER,
};

enum LastServerWidgets {
	LSW_CAPTION,
	LSW_BUTTON,
};

enum ServerButtonsWidgets {
    WID_SB_SELECT_NICE,
    WID_SB_SELECT_BTPRO,
    WID_SB_SELECT_CITYMANIA,
    WID_SB_SELECT_NONE,
    AC_SERVERS,
};

enum ServerButtonsQueryWidgets {};


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
	NICEADMINPW,
	BTPROADMINPW,
	IS_ADMIN,
};

char _inilogindata[9][MAX_COMMUNITY_STRING_LEN];

void AccountLogin(CommunityName community);
void IniReloadLogin();
void ShowServerButtons(int left,int top, int height);
void ReloadServerButtons();


bool novahost() {
	return _novahost;
}

/* for company_gui */
bool GetAdmin() { return _admin; }

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
void IniInitiate(){
	if(_inilogin != NULL) return; //it was already set
	_inilogin = new IniFile({CFG_LOGIN_KEY});
    _inilogin->LoadFromDisk(CFG_FILE, BASE_DIR);
    if (FileExists(_personal_dir + "/citymania.cfg"))
		IniReloadLogin();
}

std::string GetLoginItem(const std::string& itemname){
	IniGroup &group = _inilogin->GetOrCreateGroup(CFG_LOGIN_KEY);
	IniItem &item = group.GetOrCreateItem(itemname);
	return item.value.value_or("");
}

std::string GetServerItem(const std::string &itemname) {
    IniGroup &group = _inilogin->GetOrCreateGroup(CFG_SERVER_KEY);
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
	_inilogin->SaveToDisk(fmt::format("{}{}", _personal_dir, CFG_FILE));
    IniReloadLogin();
}

void SetServerItem(const std::string &itemname, const std::string &value) {
    IniGroup &group = _inilogin->GetOrCreateGroup(CFG_SERVER_KEY);
    IniItem &item = group.GetOrCreateItem(itemname);
    item.SetValue(value);
    _inilogin->SaveToDisk(fmt::format("{}{}", _personal_dir, CFG_FILE));
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

/* for server buttons */
/** To handle Community Server list */
class CommunityServerManager: public HTTPCallback {
 public:
	CommunityServerManager() {}

	void initiateServerSequence(const std::string &uri) {
		this->cursor = this->buf;
        this->buf_last = lastof(buf);
		NetworkHTTPSocketHandler::Connect(uri, this);
	}

	void SaveServerString() const {
		_server_list_text += this->buf;
        //NetworkClientSendChatToServer("*** data has been saved.");
        if (FindWindowByClass(CM_WC_LOGIN_WINDOW)) ReloadServerButtons();
		if (FindWindowByClass(WC_SELECT_GAME)) ReloadServerButtons();
	}

	void inspectServerData() {
		if (this->cursor - this->buf >= 4) this->SaveServerString();
		else {
			//NetworkClientSendChatToServer("*** no data has been received.");
		}
	}

	void OnFailure() override {
		ShowErrorMessage(CM_STR_SB_SERVER_LIST_UNREACHABLE, INVALID_STRING_ID,WL_ERROR);
        //NetworkClientSendChatToServer("*** connection failed.");
	}

	bool IsCancelled() const override { return false; }

	void OnReceiveData(std::unique_ptr<char[]> data, size_t length) override
	{
		if (data.get() == nullptr)
		{
			this->inspectServerData();
            this->cursor = nullptr;
		} else {
            for (size_t i = 0; i < length && this->cursor < this->buf_last; i++, this->cursor++) {
				*(this->cursor) = data.get()[i];
            }
            *(this->cursor) = '\0';
            //NetworkClientSendChatToServer("*** data received");
        }

	}

 private:
    char buf[4096]{};
	char *buf_last{};
    char *cursor{};
};

void CommunityServerManagerSend()
{
    std::int8_t _community = stoi(GetServerItem(COMMUNITY));
    std::string uri;

    if (_community == 1) {
        uri = fmt::format("http://n-ice.org/openttd/serverlist.txt");
    } else if (_community == 2) {
        uri = fmt::format("https://openttd.btpro.nl/btproservers.txt");
    } else if (_community == 3) {
        uri = fmt::format("http://altseehof.de/openttd/citymaniaservers.txt");
    }

	static CommunityServerManager servermgr{};
	servermgr.initiateServerSequence(uri);
}

void GetCommunityServerListText(){
    std::int8_t _community = stoi(GetServerItem(COMMUNITY));
	if(_fromlast == _community || _community == 0) return;
	_fromlast = _community;
	_server_list_text.clear();
	CommunityServerManagerSend();
}

bool GetCommunityServer(int number, bool findonly) {

	if(_server_list_text.empty()) return false;
    _cc_address = "";
    _cc_name = "";

    std::string server;
    std::string port;

	if(number < 10) {
		server = fmt::format("SERVER0{}", number);
        port = fmt::format("PORT0{}", number);
	} else {
		server = fmt::format("SERVER{}", number);
		port = fmt::format("PORT{}", number);
	}
	size_t posaddress = _server_list_text.find(server);
	size_t posport = _server_list_text.find(port);

	if(posaddress != std::string::npos && posport != std::string::npos){
		std::string saddress = _server_list_text.substr(posaddress + 10, _server_list_text.find(";", posaddress + 10) - posaddress - 10);
		std::string sport = _server_list_text.substr(posport + 8, _server_list_text.find(";", posport + 8) - posport - 8);

		if(saddress.compare("DISABLED") == 0) return false;
		else if(findonly) return true;

		_cc_address = saddress;
		_cc_porti = std::stoi(sport);

		return true;
	}
	else if(findonly) return false;

	ShowErrorMessage(CM_STR_SB_SERVER_LIST_ERROR_FILE, INVALID_STRING_ID,WL_ERROR);
	return false;
}
/* end of serverbuttons */

/* for login window */
class GetHTTPContent: public HTTPCallback {
public:
	GetHTTPContent() {
		this->proccessing = false;
		this->buf_last = lastof(buf);
	}
	bool proccessing = false;

	void InitiateLoginSequence(const std::string& uri) {
		if(this->proccessing) return;
		this->proccessing = true;
		this->cursor = this->buf;
		NetworkHTTPSocketHandler::Connect(uri, this);
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
		} else {
			ShowErrorMessage(CM_STR_LOGIN_ERROR_BAD_INPUT, INVALID_STRING_ID, WL_ERROR);
		}
		this->proccessing = false;
	}

	virtual ~GetHTTPContent() {
	}
private:
	char buf[HTTPBUFLEN]{};
	char *buf_last{};
	char *cursor{};
};

std::string urlencode(const std::string &s) {
    static const char lookup[]= "0123456789abcdef";
    std::stringstream e;
    for(int i=0, ix=(int)s.length(); i<ix; i++)
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
	return urlencode(base64_encode((const unsigned char *)value, (int)strlen(value)));
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
			uri = fmt::format(BTPRO_HTTP_LOGIN, btpro_encode(GetLoginItem(BTPRO_LOGIN).c_str()),btpro_encode(GetLoginItem(BTPRO_PW).c_str()));
			break;
		}
		default:
			return;
	}

	static GetHTTPContent login{};
	login.InitiateLoginSequence(uri);
}

/* login window */
struct LoginWindow : Window {
    LoginWindowQueryWidgets query_widget{};

	std::int8_t _community = stoi(GetServerItem(COMMUNITY));

	LoginWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
        if ((_community == 1) || (_community == 2) || (_community == 3))  this->InitNested(window_number);

		//no need for citymania
		if (_community == 3) {
            this->DisableWidget(LWW_ADMIN_LOGIN);
            this->DisableWidget(LWW_ADMIN_LOGOUT);
            this->DisableWidget(LWW_ADMIN_PW);
        }

	}

    void Close([[maybe_unused]] int data) override {
		CloseWindowByClass(CM_WC_SERVER_BUTTONS);
		this->Window::Close();  
    }

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
        r; widget;
		ShowServerButtons(this->left, this->top, this->height);
	}


	void SetStringParameters(int widget) const override
	{
		switch(widget){
			//username
			case LWW_USER_NAME: {
				switch (_community) {
                    case 1: SetDParamStr(0, _inilogindata[NICEUSER]); break;
					case 2: SetDParamStr(0, _inilogindata[BTPROUSER]); break;
					case 3: SetDParamStr(0, _inilogindata[NOVAPOLISUSER]); break;
				}
                break;
			}
			//password
			case LWW_USER_PW: {
                switch (_community) {
                    case 1: SetDParam(0, (GetLoginItem(NICE_PW).empty() ? CM_STR_LOGIN_WINDOW_NOT_SET : CM_STR_LOGIN_WINDOW_SET)); break;
					case 2: SetDParam(0, (GetLoginItem(BTPRO_PW).empty() ? CM_STR_LOGIN_WINDOW_NOT_SET : CM_STR_LOGIN_WINDOW_SET)); break;
					case 3: SetDParam(0, (GetLoginItem(NOVAPOLIS_PW).empty() ? CM_STR_LOGIN_WINDOW_NOT_SET : CM_STR_LOGIN_WINDOW_SET)); break;
				}
                break;
			}
			//admin password
			case LWW_ADMIN_PW: {
                switch (_community) {
                    case 1: SetDParam(0, (GetLoginItem(NICE_ADMIN_PW).empty() ? CM_STR_LOGIN_WINDOW_NOT_SET : CM_STR_LOGIN_WINDOW_SET)); break;
					case 2: SetDParam(0, (GetLoginItem(BTPRO_ADMIN_PW).empty() ? CM_STR_LOGIN_WINDOW_NOT_SET : CM_STR_LOGIN_WINDOW_SET)); break;
					//case 3: SetDParam(0, (GetLoginItem(NOVAPOLIS_PW).empty() ? CM_STR_LOGIN_WINDOW_NOT_SET : CM_STR_LOGIN_WINDOW_SET)); break;
				}
                break;
			}
			//community name
			case LWW_COMMUNITY: {
                switch (_community) {
                    case 1: SetDParam(0, CM_STR_LOGIN_WINDOW_NICE); break;
					case 2: SetDParam(0, CM_STR_LOGIN_WINDOW_BTPRO); break;
					case 3: SetDParam(0, CM_STR_LOGIN_WINDOW_CITYMANIA); break;
				}
                break;
			}
		}
	}

	void OnClick([[maybe_unused]] Point pt, int widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case LWW_USER_NAME:
				this->query_widget = (LoginWindowQueryWidgets)widget;
				ShowQueryString(STR_EMPTY, CM_STR_LOGIN_WINDOW_CHANGE_USERNAME, MAX_COMMUNITY_STRING_LEN, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case LWW_USER_PW:
				this->query_widget = (LoginWindowQueryWidgets)widget;
				ShowQueryString(STR_EMPTY, CM_STR_LOGIN_WINDOW_CHANGE_PASSWORD, MAX_COMMUNITY_STRING_LEN, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case LWW_ADMIN_PW:
				this->query_widget = (LoginWindowQueryWidgets)widget;
				ShowQueryString(STR_EMPTY, CM_STR_LOGIN_WINDOW_CHANGE_PASSWORD, MAX_COMMUNITY_STRING_LEN, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case LWW_USER_LOGIN: {
                    switch (_community) {
                    case 1: if(_networking) AccountLogin(NICE); break;
					case 2: if(_networking) AccountLogin(BTPRO); break;
					case 3: if(/*_novahost && */_networking) AccountLogin(CITYMANIA); break;
				}
				break;
			}
            case LWW_USER_LOGOUT:
				NetworkClientSendChatToServer("!logout"); break;
			case LWW_ADMIN_LOGIN: {
                    switch (_community) {
                    case 1: {
						if(_networking) NetworkClientSendChatToServer(fmt::format("!alogin {} {}", _inilogindata[NICEUSER], base64_decode(_inilogindata[NICEADMINPW])));
						break;
					}
					case 2: {
						if(_networking) NetworkClientSendChatToServer(fmt::format("!alogin {} {}", _inilogindata[BTPROUSER], base64_decode(_inilogindata[BTPROADMINPW])));
						break;
					}
				}
				break;
			}
            case LWW_ADMIN_LOGOUT:
				NetworkClientSendChatToServer("!alogout"); break;
		}
	}

	void OnQueryTextFinished(char * str) override
	{
		if (str == NULL) return;
		switch(this->query_widget){
			case LQW_USER_NAME: {
                switch (_community) {
                    case 1: SetLoginItem(NICE_LOGIN, str); break;
                    case 2: SetLoginItem(BTPRO_LOGIN, str); break;
                    case 3: SetLoginItem(NOVAPOLIS_LOGIN, str); break;
				}
				break;
			}
			case LQW_USER_PW: {
				switch (_community) {
                    case 1: {
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
                    case 2: SetLoginItem(BTPRO_PW, str); break;
                    case 3: SetLoginItem(NOVAPOLIS_PW, str); break;
				}
                break;
			}
			case LQW_ADMIN_PW: {
				switch (_community) {
					case 1: SetLoginItem(NICE_ADMIN_PW, base64_encode((const unsigned char *)str, (int)strlen(str))); break;
                    case 2: SetLoginItem(BTPRO_ADMIN_PW, base64_encode((const unsigned char *)str, (int)strlen(str))); break;
				}
                break;
			}
			default: return;
		}
		this->SetDirty();
	}
};

struct AdminCompanyButtonsWindow : Window {
    AdminCompanyButtonsQueryWidgets query_widget{};

	AdminCompanyButtonsWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc) {

		this->InitNested(window_number);

		/* disable not supported buttons for n-ice */
		if (GetServerItem(COMMUNITY) == "1") {
            this->DisableWidget(ACB_COMPANY_SUSPEND);
            this->DisableWidget(ACB_COMPANY_UNSUSPEND);
            this->DisableWidget(ACB_COMPANY_AWARNING);
        }
    }

	static void CWCompanyResetCallback(Window* w, bool confirmed)
	{
		if (confirmed) NetworkClientSendChatToServer(fmt::format("!resetcompany {}",w->window_number));
	}

	static void CWCompanyResetSpecCallback(Window* w, bool confirmed)
	{
		if (confirmed) NetworkClientSendChatToServer(fmt::format("!resetcompanyspec {}",w->window_number));
	}

	static void CWCompanyResetKickCallback(Window* w, bool confirmed)
	{
		if (confirmed) NetworkClientSendChatToServer(fmt::format("!resetcompanykick {}",w->window_number));
	}


	virtual void OnClick([[maybe_unused]] Point pt, int widget,[[maybe_unused]] int click_count)
	{
		if(!_networking) return;
		int _company = this->window_number;
		switch (widget) {
			case ACB_COMPANY_EMPTY:
				NetworkClientSendChatToServer(fmt::format("!emptycompany {}",_company));
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_RESET_KICK:
				ShowQuery(CM_STR_ACB_RESET_COMP, CM_STR_ACB_RESET_KICK_SURE, this, CWCompanyResetKickCallback);
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_RESET_SPEC:
				ShowQuery(CM_STR_ACB_RESET_COMP, CM_STR_ACB_RESET_SPEC_SURE, this, CWCompanyResetSpecCallback);
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_RESET:
				ShowQuery(CM_STR_ACB_RESET_COMP, CM_STR_ACB_RESET_COMP_SURE, this, CWCompanyResetCallback);
				MarkWholeScreenDirty();
				break;
			case ACB_RESET_COMPANY_TIMER:
				this->query_widget = ACBQ_RESET_COMPANY_TIMER;
				SetDParam(0, _company);
				ShowQueryString(STR_EMPTY, CM_STR_ACB_RESET_TIMER_VALUE, 25, this, CS_NUMERAL, QSF_NONE);
				break;
			case ACB_RESET_COMPANY_TIMER_120:
				NetworkClientSendChatToServer(fmt::format("!resetcompanytimer {} 120",_company));
				MarkWholeScreenDirty();
				break;
			case ACB_RESET_COMPANY_TIMER_CANCEL:
				NetworkClientSendChatToServer(fmt::format("!cancelresetcompany {}",_company));
				break;
			case ACB_COMPANY_LOCK:
				NetworkClientSendChatToServer(fmt::format("!lockcompany {}",_company));
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_UNLOCK:
				NetworkClientSendChatToServer(fmt::format("!unlockcompany {}",_company));
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_KNOWN:
				NetworkClientSendChatToServer(fmt::format("!known {}",_company));
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_RESET_KNOWN:
				NetworkClientSendChatToServer(fmt::format("!resetknown {}",_company));
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_MOVE_PLAYER:
				this->query_widget = ACBQ_COMPANY_MOVE_PLAYER;
				SetDParam(0, _company);
				ShowQueryString(STR_EMPTY, STR_NETWORK_SERVER_LIST_PLAYER_NAME, 250, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case ACB_COMPANY_NEWSTICKET:
				this->query_widget = ACBQ_COMPANY_NEWSTICKET;
				SetDParam(0, _company);
				ShowQueryString(STR_EMPTY, CM_STR_ACB_PLAYER_NEWSTICKET, 250, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case ACB_COMPANY_NEWSTICKET_COMP:
				this->query_widget = ACBQ_COMPANY_NEWSTICKET_COMP;
				SetDParam(0, _company);
				ShowQueryString(STR_EMPTY, CM_STR_ACB_PLAYER_NEWSTICKET, 250, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case ACB_COMPANY_SUSPEND:
				NetworkClientSendChatToServer(fmt::format("!suspend {}",_company));
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_UNSUSPEND:
				NetworkClientSendChatToServer(fmt::format("!unsuspend {}",_company));
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_AWARNING:
				NetworkClientSendChatToServer(fmt::format("!awarning {}",_company));
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_JOIN2:
				NetworkClientSendChatToServer(fmt::format("!move #{} {}",_network_own_client_id ,_company));
				MarkWholeScreenDirty();
				break;
			case ACB_COMPANY_LEAVE:
				NetworkClientRequestMove(COMPANY_SPECTATOR);
				break;
			case ACB_COMPANY_CANCEL:
				this->Close();
				break;
		}
	}

	void SetStringParameters(WidgetID widget) const override
	{
		switch (widget) {
			case ACB_COMPANY_CAPTION:
				SetDParam(0, this->window_number);
				break;
		}
	}

	void OnQueryTextFinished(char *str)
	{
		if (str == NULL) return;
		switch (this->query_widget) {
			default: NOT_REACHED();

			case ACBQ_RESET_COMPANY_TIMER:
				NetworkClientSendChatToServer(fmt::format("!resetcompanytimer {} {}",this->window_number,str));
				MarkWholeScreenDirty();
				break;
			case ACBQ_COMPANY_NEWSTICKET: {
                std::string buffer = GetString(STR_COMPANY_NAME);
				NetworkClientSendChatToServer(fmt::format("!news {}: {}", buffer, str));
				MarkWholeScreenDirty();
				break;
			}
			case ACBQ_COMPANY_NEWSTICKET_COMP: {
				std::string buffer = GetString(STR_COMPANY_NAME);
				NetworkClientSendChatToServer(fmt::format("!news {} {}", this->window_number, str));
				MarkWholeScreenDirty();
				break;
			}
			case ACBQ_COMPANY_MOVE_PLAYER: {
				NetworkClientSendChatToServer(fmt::format("!move #{} {}",str, this->window_number));
				MarkWholeScreenDirty();
				break;
			}
		}
	}

};

struct JoinLastServerWindow : Window {

    JoinLastServerWindow(WindowDesc *desc, WindowNumber window_number)
        : Window(desc) {

        this->InitNested(window_number);

		if (_settings_client.network.last_joined == "")
            this->DisableWidget(LSW_BUTTON);

		//call for intro menue
		ShowServerButtons(ls_left, ls_top, ls_height);
    }

	virtual void OnClick([[maybe_unused]] Point pt, int widget,[[maybe_unused]] int click_count)
	{
        switch (widget) {
            case LSW_BUTTON: {
				std::string _server = _settings_client.network.last_joined;
                if (_ctrl_pressed)
                    NetworkClientConnectGame(_server, COMPANY_NEW_COMPANY);
				else
					NetworkClientConnectGame(_server, COMPANY_SPECTATOR);
			}
            break;
        }
	};

};

struct ServerButtonsWindow : Window {
    ServerButtonsQueryWidgets query_widget{};

	std::int8_t _community = stoi(GetServerItem(COMMUNITY));

    ServerButtonsWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc) {

		this->InitNested(window_number);

		if(_community == 1){
            this->GetWidget<NWidgetCore>(WID_SB_SELECT_NICE)->colour = COLOUR_ORANGE;
		}
		else if(_community == 2){
			this->GetWidget<NWidgetCore>(WID_SB_SELECT_BTPRO)->colour = COLOUR_ORANGE;
		}
		else if(_community == 3){
			this->GetWidget<NWidgetCore>(WID_SB_SELECT_CITYMANIA)->colour = COLOUR_ORANGE;
		}
	}

	virtual void OnClick([[maybe_unused]] Point pt, int widget, [[maybe_unused]] int click_count)
	{
		switch (widget) {
            case WID_SB_SELECT_NICE:
                SetServerItem(COMMUNITY, "1");
				GetCommunityServerListText();
				break;
            case WID_SB_SELECT_BTPRO:
                SetServerItem(COMMUNITY, "2");
                GetCommunityServerListText();
                break;
            case WID_SB_SELECT_CITYMANIA:
                SetServerItem(COMMUNITY, "3");
                GetCommunityServerListText();
                break;
            default:
                if (widget >= AC_SERVERS) {
                    if (GetCommunityServer(widget - AC_SERVERS + 1,false)) {
                        if (_ctrl_pressed) {
                            NetworkClientConnectGame(fmt::format("{}:{}", _cc_address, _cc_porti), COMPANY_NEW_COMPANY);
                        } else {
							NetworkClientConnectGame(fmt::format("{}:{}", _cc_address, _cc_porti), COMPANY_SPECTATOR);
                        }
                    }
                } else ShowErrorMessage(CM_STR_SB_SERVER_DISABLED, INVALID_STRING_ID, WL_ERROR);
                break;
		}
	}

	static void GetServerName(int number)
	{
        size_t poscount = _server_list_text.find("servercount");
		std::string scount = _server_list_text.substr(poscount + 12, 3);
        _servercount = std::stoi(scount);

		std::string name;
		if (number < 10){
			name = fmt::format("NAME0{}",number);
		} else {
			name = fmt::format("NAME{}",number);
		}
		std::string server;
		if (number < 10){
			server = fmt::format("SERVER0{}",number);
		} else {
			server = fmt::format("SERVER{}",number);
		}
		size_t posname = _server_list_text.find(name);
		std::string sname = _server_list_text.substr(posname + 8, _server_list_text.find(";", posname + 8) - posname - 8);

		if (number > _servercount)
                    _cc_name = "-";
                else
                    _cc_name = sname;
	};

	virtual void DrawWidget(const Rect &r, int widget) const override
	{
		std::string name;

		switch (widget) {
            case AC_SERVERS:
			default:
				if(widget >= AC_SERVERS){
					if(widget - AC_SERVERS + 1 < 10){
                        name = fmt::format("NAME0{}",widget - AC_SERVERS +1);
					}
					else {
						name = fmt::format("NAME{}",widget - AC_SERVERS +1);
					}
					size_t posname = _server_list_text.find(name);
					std::string sname = _server_list_text.substr(posname + 8, _server_list_text.find(";", posname + 8) - posname - 8);
					_cc_name = sname;

					SetDParamStr(0, _cc_name);
					DrawString(r.left, r.right, r.top + 3, CM_STR_SB_NETWORK_DIRECT_JOIN_GAME, TC_FROMSTRING, SA_CENTER);
				}
				break;
		}
	}
};

std::unique_ptr<NWidgetBase> MakeServerButtons()
{
	std::int8_t _community = stoi(GetServerItem(COMMUNITY));
	auto ver = std::make_unique<NWidgetVertical>();

	if(_community == 0 || _server_list_text.empty()){
		auto leaf = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_GREY, AC_SERVERS);
		ver->Add(std::move(leaf));
		return ver;
	}

    /* check for disabled server from serverlist file */
        int active = 0, aactive[50]{}, s_max = 0;
	if (_community == 1) s_max = 50; //for n-ice
	if (_community == 2) s_max = 30; //for btpro
    if (_community == 3) s_max = 10; //for citymania
	for (int i = 0; i < s_max; i++) {
        aactive[i] = GetCommunityServer(i + 1, true) ? (i + 1) : 0; //server disabled?
        active++;
    }

	auto hor = std::make_unique<NWidgetHorizontal>();
	int i1 = 0, i2 = 0;
	for (int i = 0; i < s_max; i++) {
		if ((aactive[i] == 0) && (_community == 1)) continue; //hide button if disabled - for n-ice only
		i2++;
		if ((i1 == 5) || (i1 == 10) || (i1 == 15) || (i1 == 20) || (i1 == 25) || (i1 == 30) || (i1 == 35) || (i1 == 40) || (i1 == 45) || (i1 == 50)) {
			i2=0;
			auto spce = std::make_unique<NWidgetSpacer>(3, 0);
			spce->SetFill(1, 0);
			hor->Add(std::move(spce));
			ver->Add(std::move(hor));
			auto spc = std::make_unique<NWidgetSpacer>(0, 4);
			spc->SetFill(1, 0);
			ver->Add(std::move(spc));
			hor = std::make_unique<NWidgetHorizontal>();
		}
		auto spce = std::make_unique<NWidgetSpacer>(4, 0);
		spce->SetFill(1, 0);
		hor->Add(std::move(spce));
		auto leaf = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_ORANGE, AC_SERVERS + i);
		if(aactive[i] == 0) leaf->SetDisabled(true);
		leaf->SetDataTip(CM_STR_SB_NETWORK_DIRECT_JOIN_GAME, CM_STR_SB_NETWORK_DIRECT_JOIN_TOOLTIP);
		leaf->SetMinimalSize(79, 15);
		hor->Add(std::move(leaf));
		i1++;
	}

	/* arrange buttons @ last line */
	if (i2==0) i2=375;
	if (i2==1) i2=282;
	if (i2==2) i2=189;
	if (i2==3) i2=96;
	if (i2==4) i2=3;
	auto spce = std::make_unique<NWidgetSpacer>(i2, 0);
	spce->SetFill(1, 0);
	hor->Add(std::move(spce));
	ver->Add(std::move(hor));
	return ver;
}


static const NWidgetPart _nested_login_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_ORANGE),
		NWidget(WWT_CAPTION, COLOUR_ORANGE), SetDataTip(CM_STR_LOGIN_WINDOW_CAPTION, 0),
	EndContainer(),
    NWidget(WWT_PANEL, COLOUR_BROWN), SetFill(0,1),
		NWidget(NWID_VERTICAL, NC_EQUALSIZE),
			//welcome
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetMinimalSize(112, 0), SetFill(1, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_COMMUNITY), SetMinimalSize(200, 40), SetAlignment(SA_CENTER), SetDataTip(CM_STR_LOGIN_WINDOW_WELCOME, 0), SetFill(1, 1),
				NWidget(NWID_SPACER), SetMinimalSize(112, 0), SetFill(1, 0),
				EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),
			//username and pw
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetMinimalSize(45, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_USERNAME), SetDataTip(CM_STR_LOGIN_WINDOW_USERNAME, 0),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_USER_NAME), SetMinimalSize(100, 15), SetFill(1, 1),
				SetDataTip(CM_STR_LOGIN_WINDOW_USERNAME_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_USERNAME_HELPTEXT),
                NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_PASSWORD), SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD, 0),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_USER_PW), SetMinimalSize(50, 15), SetFill(1, 1),
				SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_PASSWORD_HELPTEXT),
				NWidget(NWID_SPACER), SetMinimalSize(45, 0),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 20),
			//login and logout
            NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetMinimalSize(105, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, LWW_USER_LOGIN), SetMinimalSize(40, 20), SetAlignment(SA_CENTER), SetFill(1, 1),
				SetDataTip(CM_STR_TOOLBAR_COMMANDS_LOGIN_CAPTION, CM_STR_TOOLBAR_COMMANDS_LOGIN_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(10, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, LWW_USER_LOGOUT), SetMinimalSize(40, 20), SetAlignment(SA_CENTER), SetFill(1, 1),
				SetDataTip(CM_STR_TOOLBAR_COMMANDS_LOGOUT_CAPTION, CM_STR_TOOLBAR_COMMANDS_LOGOUT_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(105, 0),
            EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_admin_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_ORANGE),
		NWidget(WWT_CAPTION, COLOUR_ORANGE), SetDataTip(CM_STR_LOGIN_WINDOW_CAPTION, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN), SetFill(0, 1),
		NWidget(NWID_VERTICAL),
			//welcome
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetMinimalSize(112, 0), SetFill(1, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_COMMUNITY), SetMinimalSize(200, 40), SetAlignment(SA_CENTER), SetDataTip(CM_STR_LOGIN_WINDOW_WELCOME, 0), SetFill(1, 1),
				NWidget(NWID_SPACER), SetMinimalSize(112, 0), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			//username and pw
			NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetMinimalSize(15, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_USERNAME), SetDataTip(CM_STR_LOGIN_WINDOW_USERNAME, 0),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_USER_NAME), SetMinimalSize(100, 15), SetFill(1, 1),
				SetDataTip(CM_STR_LOGIN_WINDOW_USERNAME_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_USERNAME_HELPTEXT),
                NWidget(NWID_SPACER), SetMinimalSize(10, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_PASSWORD), SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD, 0),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_USER_PW), SetMinimalSize(50, 15), SetFill(1, 1),
				SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_PASSWORD_HELPTEXT),
				NWidget(NWID_SPACER), SetMinimalSize(10, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_ADMIN_PW), SetDataTip(CM_STR_LOGIN_WINDOW_ADMIN_PASSWORD, 0),
				NWidget(NWID_SPACER), SetMinimalSize(5, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_ADMIN_PW), SetMinimalSize(50, 15), SetFill(1, 1),
				SetDataTip(CM_STR_LOGIN_WINDOW_PASSWORD_DISPLAY, CM_STR_LOGIN_WINDOW_CHANGE_PASSWORD_HELPTEXT),
				NWidget(NWID_SPACER), SetMinimalSize(15, 0),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 20),
			//login and logout
            NWidget(NWID_HORIZONTAL),
				NWidget(NWID_SPACER), SetMinimalSize(15, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, LWW_USER_LOGIN), SetMinimalSize(40, 20), SetAlignment(SA_CENTER), SetFill(1, 1),
				SetDataTip(CM_STR_TOOLBAR_COMMANDS_LOGIN_CAPTION, CM_STR_TOOLBAR_COMMANDS_LOGIN_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(10, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, LWW_USER_LOGOUT), SetMinimalSize(40, 20), SetAlignment(SA_CENTER), SetFill(1, 1),
				SetDataTip(CM_STR_TOOLBAR_COMMANDS_LOGOUT_CAPTION, CM_STR_TOOLBAR_COMMANDS_LOGOUT_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(10, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, LWW_ADMIN_LOGIN), SetMinimalSize(40, 20), SetAlignment(SA_CENTER), SetFill(1, 1),
				SetDataTip(CM_STR_LOGIN_WINDOW_ADMIN_LOGIN, CM_STR_TOOLBAR_COMMANDS_LOGIN_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(10, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, LWW_ADMIN_LOGOUT), SetMinimalSize(40, 20), SetAlignment(SA_CENTER), SetFill(1, 1),
				SetDataTip(CM_STR_LOGIN_WINDOW_ADMIN_LOGOUT, CM_STR_TOOLBAR_COMMANDS_LOGOUT_TOOLTIP),
				NWidget(NWID_SPACER), SetMinimalSize(15, 0),
            EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_admin_company_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
    NWidget(WWT_CAPTION,COLOUR_END, ACB_COMPANY_CAPTION),
		SetDataTip(CM_STR_ACB_COMPANY_ADMIN_CAPTION, 0),
		SetMinimalSize(10,17),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetFill(0, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetMinimalSize(5, 0), SetFill(1, 0),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(5, 3, 5),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_LOCK), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_LOCK, CM_STR_ACB_LOCK_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_UNLOCK), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_UNLOCK, CM_STR_ACB_UNLOCK_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_KNOWN), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_KNOWN, CM_STR_ACB_KNOWN_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_RESET_KNOWN), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_RESET_KNOWN, CM_STR_ACB_RESET_KNOWN_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_MOVE_PLAYER), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_MOVE_PLAYER_TO, CM_STR_ACB_MOVE_PLAYER_TO_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_NEWSTICKET_COMP), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_COMPANY_NEWSTICKET_BUTTON_COMP, CM_STR_ACB_COMPANY_NEWSTICKET_BUTTON_COMP_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_SUSPEND), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_SUSPEND, CM_STR_ACB_SUSPEND_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_UNSUSPEND), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_UNSUSPEND, CM_STR_ACB_UNSUSPEND_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_JOIN2),SetMinimalSize(10, 13), SetFill(1, 0),SetDataTip(CM_STR_ACB_COMPANY_JOIN2, CM_STR_ACB_COMPANY_JOIN2_TOOLTIP),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(5, 0), SetFill(1, 0),
			NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPIP(5, 3, 5),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_EMPTY), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_EMPTY, CM_STR_ACB_EMPTY_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_RESET), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_RESET, CM_STR_ACB_RESET_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_RESET_SPEC), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_RESET_SPEC, CM_STR_ACB_RESET_SPEC_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_RESET_KICK), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_RESET_KICK, CM_STR_ACB_RESET_KICK_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_RESET_COMPANY_TIMER_120), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_RESET_TIMER_120, CM_STR_ACB_RESET_TIMER_120_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_RESET_COMPANY_TIMER), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_RESET_TIMER, CM_STR_ACB_RESET_TIMER_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_RESET_COMPANY_TIMER_CANCEL), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_RESET_TIMER_CANCEL, CM_STR_ACB_RESET_TIMER_CANCEL_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_AWARNING), SetMinimalSize(10, 13), SetFill(1, 0), SetDataTip(CM_STR_ACB_AWARNING, CM_STR_ACB_AWARNING_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, ACB_COMPANY_LEAVE),SetMinimalSize(10, 13), SetFill(1, 0),SetDataTip(STR_NETWORK_COMPANY_LIST_SPECTATE, STR_NETWORK_COMPANY_LIST_SPECTATE),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(5, 0), SetFill(1, 0),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 5), SetFill(0, 1),
	EndContainer(),
};

static const NWidgetPart _nested_last_server_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_BROWN), SetFill(0, 1),
		NWidget(NWID_HORIZONTAL), SetPadding(WidgetDimensions::unscaled.sparse),
			NWidget(NWID_SPACER), SetMinimalSize(81, 0), SetFill(1, 0),
			NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, LSW_BUTTON), SetMinimalSize(242, 15), SetAlignment(SA_CENTER), SetDataTip(STR_NETWORK_SERVER_LIST_CLICK_TO_SELECT_LAST, CM_STR_SB_NETWORK_DIRECT_JOIN_TOOLTIP), SetFill(1, 1),
			NWidget(NWID_SPACER), SetMinimalSize(81, 0), SetFill(1, 0),
		EndContainer(),
	EndContainer(),
};

static const NWidgetPart _nested_server_buttons_window_widgets[] = {
	NWidget(WWT_PANEL, COLOUR_BROWN), SetFill(0, 1),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE), SetPadding(4), SetPIP(0, 7, 0),
 				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SB_SELECT_NICE), SetMinimalSize(134, 13), SetDataTip(CM_STR_SB_SELECT_NICE, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SB_SELECT_BTPRO), SetMinimalSize(134, 13), SetDataTip(CM_STR_SB_SELECT_BTPRO, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_SB_SELECT_CITYMANIA), SetMinimalSize(134, 13), SetDataTip(CM_STR_SB_SELECT_CITYMANIA, 0),
 		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
		NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
			NWidgetFunction(MakeServerButtons),
		EndContainer(),
		NWidget(NWID_SPACER), SetMinimalSize(0, 3),
    EndContainer(),
};

static WindowDesc _login_window_desc(__FILE__, __LINE__,
	WDP_CENTER, "cm_login", 0, 0,
	CM_WC_LOGIN_WINDOW, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_login_window_widgets), std::end(_nested_login_window_widgets)
);

static WindowDesc _admin_window_desc(__FILE__, __LINE__,
	WDP_CENTER, "cm_login", 0, 0,
	CM_WC_LOGIN_WINDOW, WC_NONE,
	WDF_CONSTRUCTION,
	std::begin(_nested_admin_window_widgets), std::end(_nested_admin_window_widgets)
);

static WindowDesc _admin_company_buttons_desc(__FILE__, __LINE__,
	WDP_AUTO, NULL, 0, 0,
    CM_WC_ADMIN_COMPANY_BUTTONS, WC_NONE,
	WDF_CONSTRUCTION,
    std::begin(_nested_admin_company_window_widgets), std::end(_nested_admin_company_window_widgets)
);

static WindowDesc _last_server_desc(__FILE__, __LINE__,
	WDP_AUTO, NULL, 0, 0,
	CM_LAST_SERVER,
    WC_NONE, WDF_CONSTRUCTION,
	std::begin(_nested_last_server_widgets), std::end(_nested_last_server_widgets));

static WindowDesc _server_buttons_desc(__FILE__, __LINE__,
	WDP_AUTO, NULL, 0, 0,
    CM_WC_SERVER_BUTTONS, WC_NONE,
	WDF_CONSTRUCTION,
    std::begin(_nested_server_buttons_window_widgets), std::end(_nested_server_buttons_window_widgets)
);

/* Identify the current community */
void CheckCommunity() {
    if (_networking) {
        if (_network_server_name.find("n-ice.org") != std::string::npos) {
            if (GetServerItem(COMMUNITY) != "1") {
                SetServerItem(COMMUNITY, "1");
                GetCommunityServerListText();
            }
        } else if (_network_server_name.find("BTPro.nl") != std::string::npos) {
            if (GetServerItem(COMMUNITY) != "2") {
                SetServerItem(COMMUNITY, "2");
                GetCommunityServerListText();
            }
        } else if (_network_server_name.find("CityMania.org") !=
                   std::string::npos) {
            if (GetServerItem(COMMUNITY) != "3") {
                SetServerItem(COMMUNITY, "3");
                GetCommunityServerListText();
            }
        } else if (_network_server_name.find("reddit.com") !=
                   std::string::npos) {
            if (GetServerItem(COMMUNITY) != "4") {
                SetServerItem(COMMUNITY, "4");
                // GetCommunityServerListText();
            }
        } else {  // Unknown Server
            SetServerItem(COMMUNITY, "0");
        }
    }
    /* for intro menue */
    if ((GetServerItem(COMMUNITY) == "1") || (GetServerItem(COMMUNITY) == "2") || (GetServerItem(COMMUNITY) == "3"))
        GetCommunityServerListText();
};

void CheckAdmin() {
    IniInitiate();
    if (GetLoginItem(ADMIN) == "1")
        _admin = true;
};

void ShowLoginWindow() {
    IniInitiate();
    CheckCommunity();
    CheckAdmin();
    CloseWindowByClass(CM_WC_LOGIN_WINDOW);
    if (!_admin) AllocateWindowDescFront<LoginWindow>(&_login_window_desc, 0);
    else AllocateWindowDescFront<LoginWindow>(&_admin_window_desc, 0);
};

void ShowServerButtons(int left, int top, int height) {

	_left = left;
    _top = top;
    _height = height;

	IniInitiate();
    if (!FileExists(_personal_dir + "/citymania.cfg"))
		return;

	/* create window at coordinates */
    Window *b;
    if (FindWindowByClass(CM_WC_SERVER_BUTTONS))
		CloseWindowByClass(CM_WC_SERVER_BUTTONS);
    b = new ServerButtonsWindow(&_server_buttons_desc, 0);
    b->top = top+height;
    b->left = left;
    b->SetDirty();
};

void ReloadServerButtons() {
	ShowServerButtons(_left, _top, _height);
}

void CreateCommunityServerList() {
    IniInitiate();
    CheckCommunity();
};

void ShowAdminCompanyButtons(int left, int top, int width, int company2, bool draw, bool redraw) {
    if (!draw) {
        if (FindWindowById(CM_WC_ADMIN_COMPANY_BUTTONS,company2))
			CloseWindowById(CM_WC_ADMIN_COMPANY_BUTTONS, company2);
        /* clear for company */
        ACB_Location[company2 - 1][0]=0;
        ACB_Location[company2 - 1][1]=0;
        ACB_Location[company2 - 1][2]=0;
        return;
    }
	if (!Company::IsValidID((CompanyID)(company2-1))) return;
        if ((left == ACB_Location[company2 - 1][0]) &&
            (top == ACB_Location[company2 - 1][1]) &&
            (width == ACB_Location[company2 - 1][2]) &&
			(!redraw))
            return;

	/* save position of company window */
    ACB_Location[company2 - 1][0] = left;
    ACB_Location[company2 - 1][1] = top;
    ACB_Location[company2 - 1][2] = width;
    if (FindWindowById(CM_WC_ADMIN_COMPANY_BUTTONS,company2))
		CloseWindowById(CM_WC_ADMIN_COMPANY_BUTTONS, company2);
    Window *w;
    w = new AdminCompanyButtonsWindow(&_admin_company_buttons_desc, company2);
    w->top = top;
    w->left = left + width;
    w->SetDirty();
};

/* last server widget */
void JoinLastServer(int left, int top, int height) {
    if (FindWindowByClass (CM_LAST_SERVER))
		CloseWindowByClass(CM_LAST_SERVER);
    Window *d;
    d = new JoinLastServerWindow(&_last_server_desc, 0);
    d->top = top + height;
    d->left = left;
    ls_left = left;
    ls_top = d->top;
    ls_height = d->height;
}


} // namespace citymania
