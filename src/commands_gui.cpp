/* $Id: commands_gui.cpp 21909 2011-01-26 08:14:36Z xi $ */

#ifdef ENABLE_NETWORK
#include "stdafx.h"
#include "widgets/dropdown_type.h" //fillrect
#include "core/geometry_func.hpp" //maxdim
#include "settings_type.h"
#include "settings_func.h" //saveconfig
#include "3rdparty/md5/md5.h" //pass crypt
#include "network/network_func.h" //network chat
#include "textbuf_gui.h"  //show query
#include "network/network.h" //networking
#include "network/core/tcp_http.h" //http connector
#include "ini_type.h" //ini file
#include "fileio_func.h" //personal dir
#include "error.h" //error message
#include "debug.h"

bool _novahost = false;
IniFile *_inilogin = NULL;

static const int HTTPBUFLEN = 128;

static const char * const NOVAPOLIS_IPV4_PRIMARY   = "37.157.196.78";
static const char * const NOVAPOLIS_IPV6_PRIMARY   = "2a02:2b88:2:1::1d73:1";
static const char * const NOVAPOLIS_IPV4_SECONDARY = "89.111.65.225";
static const char * const NOVAPOLIS_IPV6_SECONDARY = "fe80::20e:7fff:fe23:bee0";
static const char * const NOVAPOLIS_STRING         = "novapolis";
static const char * const NICE_HTTP_LOGIN          = "http://n-ice.org/openttd/gettoken.php?user=%s&password=%s";
static const char * const BTPRO_HTTP_LOGIN         = "http://openttd.btpro.nl/gettoken.php?user=%s&password=%s";

static const char * const NOVA_IP_ADDRESSES[] = {
	NOVAPOLIS_IPV4_PRIMARY,
	NOVAPOLIS_IPV6_PRIMARY,
	NOVAPOLIS_IPV4_SECONDARY,
	NOVAPOLIS_IPV6_SECONDARY,
};

static const char * const CFG_LOGIN_FILE  = "novapolis.cfg";
static const char * const CFG_LOGIN_KEY   = "login";
static const char * const NOVAPOLIS_LOGIN = "novapolis_login";
static const char * const NOVAPOLIS_PW    = "novapolis_pw";
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
	NOVAPOLIS,
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

char _inilogindata[6][64];

void ShowLoginWindow();
void AccountLogin(CommunityName community);
void IniReloadLogin();
char * GetLoginItem(const char * item);

bool novahost(){
	_novahost = false;
	for(int i = 0, len = lengthof(NOVA_IP_ADDRESSES); i < len; i++){
		if(strcmp(_settings_client.network.last_host, NOVA_IP_ADDRESSES[i]) == 0){
			_novahost = true;
			break;
		}
	}
	if(_novahost == false && strstr(_settings_client.network.last_host, NOVAPOLIS_STRING) != NULL){
		_novahost = true;
	}
	return _novahost;
}

void strtomd5(char * buf, char * bufend, int length){
	uint8 digest[16];
	Md5 checksum;
	checksum.Append(buf, length);
	checksum.Finish(digest);
	md5sumToString(buf, bufend, digest);
	strtolower(buf);
}

void UrlEncode(char * buf, const char * buflast, const char * url){
	while(*url != '\0' && buf < buflast){
		if((*url >= '0' && *url <= '9') || (*url >= 'A' && *url <= 'Z') || (*url >= 'a' && *url <= 'z')
			|| *url == '-' || *url == '_' || *url == '.' || *url == '~'
		){
			*buf++ = *url++;
		}
		else{
			buf += seprintf(buf, buflast, "%%%02X", *url++);
		}
	}
	*buf = '\0';
}

/** Commands toolbar window handler. */
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

					NetworkClientConnectGame(NetworkAddress(ip, (3980 + widget - CTW_NS0)), COMPANY_SPECTATOR);
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
		d.width += WD_FRAMERECT_LEFT + WD_FRAMERECT_RIGHT;
		d.height += WD_FRAMERECT_TOP + WD_FRAMERECT_BOTTOM;
		*size = maxdim(d, *size);
	}

	void DrawWidget(const Rect &r, int widget) const
	{
		if (widget < CTW_CARGO_FIRST) return;

		const CargoSpec *cs = _sorted_cargo_specs[widget - CTW_CARGO_FIRST];
		bool rtl = _current_text_dir == TD_RTL;

		/* Since the buttons have no text, no images,
		 * both the text and the coloured box have to be manually painted.
		 * clk_dif will move one pixel down and one pixel to the right
		 * when the button is clicked */
		byte clk_dif = this->IsWidgetLowered(widget) ? 1 : 0;
		int x = r.left + WD_FRAMERECT_LEFT;
		int y = r.top;

		int rect_x = clk_dif + (rtl ? r.right - 12 : r.left + WD_FRAMERECT_LEFT);

		GfxFillRect(rect_x, y + clk_dif, rect_x + 8, y + 5 + clk_dif, 0);
		GfxFillRect(rect_x + 1, y + 1 + clk_dif, rect_x + 7, y + 4 + clk_dif, cs->legend_colour);
		SetDParam(0, cs->name);
		DrawString(rtl ? r.left : x + 14 + clk_dif, (rtl ? r.right - 14 + clk_dif : r.right), y + clk_dif, STR_GRAPH_CARGO_PAYMENT_CARGO);
	}

	void OnHundredthTick()
	{
		for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
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

/** Construct the row containing the digit keys. */
static NWidgetBase *MakeCargoButtons(int *biggest_index)
{
	NWidgetVertical *ver = new NWidgetVertical;

	for (int i = 0; i < _sorted_standard_cargo_specs_size; i++) {
		NWidgetBackground *leaf = new NWidgetBackground(WWT_PANEL, COLOUR_ORANGE, CTW_CARGO_FIRST + i, NULL);
		leaf->tool_tip = STR_GRAPH_CARGO_PAYMENT_TOGGLE_CARGO;
		leaf->SetFill(1, 0);
		ver->Add(leaf);
	}
	*biggest_index = CTW_CARGO_FIRST + _sorted_standard_cargo_specs_size - 1;
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
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS1), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS2), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS3), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS4), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS5), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS6), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS7), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS9), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX1), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX2), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX3), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX4), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX5), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX6), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX7), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
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
);

void ShowCommandsToolbar()
{
	DeleteWindowByClass(WC_COMMAND_TOOLBAR);
	AllocateWindowDescFront<CommandsToolbarWindow>(&_commands_toolbar_desc, 0);
}

// login window
class GetHTTPContent: public HTTPCallback {
public:
	GetHTTPContent(char *uri): uri(uri) {
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

	virtual void OnReceiveData(const char *data, size_t length) {
		if (data == NULL) {
			this->cursor = NULL;
			this->LoginAlready();
		}
		else {
			for(size_t i=0; i<length && this->cursor < this->buf_last; i++, data++, this->cursor++) {
				*(this->cursor) = *data;
			}
			*(this->cursor) = '\0';
		}
	}

	virtual void OnFailure() {
		ShowErrorMessage(STR_LOGINERROR_NOCONNECT, INVALID_STRING_ID, WL_ERROR);
	}

	void LoginAlready(){
		if(strlen(this->buf) == 4 && _networking){
			char msg[32];
			seprintf(msg, lastof(msg), "!login %s", this->buf);
			NetworkClientSendChatToServer(msg);
		}
		else{
			ShowErrorMessage(STR_LOGINERROR_BADINPUT, INVALID_STRING_ID, WL_ERROR);
		}
		this->proccessing = false;
	}

	virtual ~GetHTTPContent() {
	}
private:
	NetworkHTTPContentConnecter *conn;
	char buf[HTTPBUFLEN];
	char *buf_last;
	char *cursor;
	char *uri;
};
//send login
void AccountLogin(CommunityName community){
	char uri[128];
	switch(community){
		case NOVAPOLIS:
			seprintf(uri, lastof(uri), "!login %s %s", _inilogindata[NOVAPOLISUSER], _inilogindata[NOVAPOLISPW]);
			NetworkClientSendChatToServer(uri);
			return;
		case NICE:
			seprintf(uri, lastof(uri), NICE_HTTP_LOGIN, GetLoginItem(NICE_LOGIN), GetLoginItem(NICE_PW));
			break;
		case BTPRO:
			seprintf(uri, lastof(uri), BTPRO_HTTP_LOGIN, GetLoginItem(BTPRO_LOGIN), GetLoginItem(BTPRO_PW));
			break;
		default:
			return;
	}
	static GetHTTPContent login(uri);
	login.InitiateLoginSequence();
}
#endif /* ENABLE_NETWORK */

//ini login hadling
void IniLoginInitiate(){
	static const char * const list_login[] = {
		CFG_LOGIN_KEY,
		NULL
	};

	if(_inilogin != NULL) return; //it was already set
	_inilogin = new IniFile(list_login);
	_inilogin->LoadFromDisk(CFG_LOGIN_FILE, BASE_DIR);
	IniReloadLogin();
}

void IniReloadLogin(){
	char str[64];
	const char * itemvalue;
	for(int i = 0, len = lengthof(INI_LOGIN_KEYS); i < len; i++){
		itemvalue = GetLoginItem(INI_LOGIN_KEYS[i]);
		if(itemvalue == NULL){
			GetString(str, STR_LOGIN_NOTSET, lastof(str));
		}
		else{
			strecpy(str, itemvalue, lastof(str));
		}
		strecpy(_inilogindata[i], str, lastof(_inilogindata[i]));
	}
}

char * GetLoginItem(const char * itemname){
	IniGroup *group = _inilogin->GetGroup(CFG_LOGIN_KEY);
	if(group == NULL) return NULL;
	IniItem *item = group->GetItem(itemname, true);
	if(item == NULL || item->value == NULL) return NULL;
	return item->value;
}

void SetLoginItem(const char * itemname, const char * value){
	IniGroup *group = _inilogin->GetGroup(CFG_LOGIN_KEY);
	if(group == NULL) return;
	IniItem *item = group->GetItem(itemname, true);
	if(item == NULL) return;
	item->SetValue(value);
	_inilogin->SaveToDisk(str_fmt("%s%s", _personal_dir, CFG_LOGIN_FILE));
	IniReloadLogin();
}
//login window
struct LoginWindow : Window {
	LoginWindowQueryWidgets query_widget;

	LoginWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		if(_novahost || !_networking){
			this->DisableWidget(LWW_NICE);
			this->DisableWidget(LWW_BTPRO);
		}
		if(!_novahost || !_networking) this->DisableWidget(LWW_NOVAPOLIS);
	}

	virtual void SetStringParameters(int widget) const
	{
		switch(widget){
			case LWW_NOVAPOLIS_LOGIN:
				SetDParamStr(0, _inilogindata[NOVAPOLISUSER]);
				break;
			case LWW_NOVAPOLIS_PW:
				SetDParam(0, (GetLoginItem(NOVAPOLIS_PW) == NULL ? STR_LOGIN_NOTSET : STR_LOGIN_SET));
				break;
			case LWW_NICE_LOGIN:
				SetDParamStr(0, _inilogindata[NICEUSER]);
				break;
			case LWW_NICE_PW:
				SetDParam(0, (GetLoginItem(NICE_PW) == NULL ? STR_LOGIN_NOTSET : STR_LOGIN_SET));
				break;
			case LWW_BTPRO_LOGIN:
				SetDParamStr(0, _inilogindata[BTPROUSER]);
				break;
			case LWW_BTPRO_PW:
				SetDParam(0, (GetLoginItem(BTPRO_PW) == NULL ? STR_LOGIN_NOTSET : STR_LOGIN_SET));
				break;
		}
	}

	virtual void OnClick(Point pt, int widget, int click_count)
	{
		switch (widget) {
			case LWW_NOVAPOLIS:
				if(_novahost && _networking) AccountLogin(NOVAPOLIS);
				break;
			case LWW_NICE:
				if(_networking) AccountLogin(NICE);
				break;
			case LWW_BTPRO:
				if(_networking) AccountLogin(BTPRO);
				break;
			case LWW_NOVAPOLIS_LOGIN:
			case LWW_NOVAPOLIS_PW:
			case LWW_NICE_LOGIN:
			case LWW_NICE_PW:
			case LWW_BTPRO_LOGIN:
			case LWW_BTPRO_PW:
				this->query_widget = (LoginWindowQueryWidgets)widget;
				ShowQueryString(STR_EMPTY, STR_LOGIN_CHANGE_USERNAME, 32, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
		}
	}

	void OnQueryTextFinished(char * str)
	{
		if (str == NULL) return;
		char item[128];
		switch(this->query_widget){
			case LQW_NOVAPOLIS_LOGIN: {
				SetLoginItem(NOVAPOLIS_LOGIN, str);
				break;
			}
			case LQW_NOVAPOLIS_PW: {
				char msg[128];
				strecpy(msg, str, lastof(msg));
				strtomd5(msg, lastof(msg), (int)strlen(msg));
				SetLoginItem(NOVAPOLIS_PW, msg);
				break;
			}
			case LQW_NICE_LOGIN:
			case LQW_NICE_PW:
			case LQW_BTPRO_LOGIN:
			case LQW_BTPRO_PW:
				UrlEncode(item, lastof(item), str);
				SetLoginItem(INI_LOGIN_KEYS[this->query_widget - 3], item); // - LWW_NICE_LOGIN + NICE_LOGIN
				break;
			default: return;
		}
		this->SetDirty();
	}
};

static const NWidgetPart _nested_login_window_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetDataTip(STR_LOGINWINDOW_CAPTION, 0),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 0),
		NWidget(NWID_VERTICAL, NC_EQUALSIZE), SetPadding(10),
			//novapolis
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_USERNAME), SetDataTip(STR_LOGIN_USERNAME, 0),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_PASSWORD), SetDataTip(STR_LOGIN_PASSWORD, 0),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_NOVAPOLIS_LOGIN), SetMinimalSize(60, 20), SetFill(1, 0), SetDataTip(STR_LOGIN_USERNAME_DISPLAY, STR_LOGIN_CHANGE_USERNAME),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_NOVAPOLIS_PW), SetMinimalSize(30, 20), SetFill(1, 0), SetDataTip(STR_LOGIN_PASSWORD_DISPLAY, STR_LOGIN_CHANGE_PASSWORD),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			NWidget(WWT_PUSHTXTBTN, COLOUR_PURPLE, LWW_NOVAPOLIS), SetMinimalSize(100, 30), SetFill(1, 0), SetDataTip(STR_LOGINWINDOW_NOVAPOLIS, STR_LOGIN_SEND_LOGIN_TT),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			//n-ice
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_USERNAME), SetDataTip(STR_LOGIN_USERNAME, 0),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_PASSWORD), SetDataTip(STR_LOGIN_PASSWORD, 0),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_NICE_LOGIN), SetMinimalSize(60, 20), SetFill(1, 0), SetDataTip(STR_LOGIN_USERNAME_DISPLAY, STR_LOGIN_CHANGE_USERNAME),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_NICE_PW), SetMinimalSize(30, 20), SetFill(1, 0), SetDataTip(STR_LOGIN_PASSWORD_DISPLAY, STR_LOGIN_CHANGE_PASSWORD),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			NWidget(WWT_PUSHTXTBTN, COLOUR_LIGHT_BLUE, LWW_NICE), SetMinimalSize(100, 30), SetFill(1, 0), SetDataTip(STR_LOGINWINDOW_NICE, STR_LOGIN_SEND_LOGIN_TT),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			//btpro
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_USERNAME), SetDataTip(STR_LOGIN_USERNAME, 0),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_TEXT, COLOUR_BROWN, LWW_PASSWORD), SetDataTip(STR_LOGIN_PASSWORD, 0),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_BTPRO_LOGIN), SetMinimalSize(60, 20), SetFill(1, 0), SetDataTip(STR_LOGIN_USERNAME_DISPLAY, STR_LOGIN_CHANGE_USERNAME),
				NWidget(NWID_SPACER), SetMinimalSize(20, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, LWW_BTPRO_PW), SetMinimalSize(30, 20), SetFill(1, 0), SetDataTip(STR_LOGIN_PASSWORD_DISPLAY, STR_LOGIN_CHANGE_PASSWORD),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 10),
			NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, LWW_BTPRO), SetMinimalSize(100, 30), SetFill(1, 0), SetDataTip(STR_LOGINWINDOW_BTPRO, STR_LOGIN_SEND_LOGIN_TT),
		EndContainer(),
	EndContainer(),
};

static WindowDesc _login_window_desc(
	WDP_CENTER, NULL, 0, 0,
	WC_LOGIN_WINDOW, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_login_window_widgets, lengthof(_nested_login_window_widgets)
);

void ShowLoginWindow()
{
	IniLoginInitiate();
	DeleteWindowByClass(WC_LOGIN_WINDOW);
	AllocateWindowDescFront<LoginWindow>(&_login_window_desc, 0);
}
