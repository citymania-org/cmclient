/* $Id: commands_gui.cpp 21909 2011-01-26 08:14:36Z xi $ */

#ifdef ENABLE_NETWORK
#include "stdafx.h"
#include "window_gui.h"
#include "window_func.h"
#include "strings_func.h"
#include "widgets/dropdown_type.h"
#include "core/geometry_func.hpp"
#include "settings_type.h"
#include "settings_func.h" //saveconfig
#include "3rdparty/md5/md5.h"
#include "table/strings.h"
#include "network/network_type.h"
#include "network/network_func.h"
#include "textbuf_gui.h"
#include "cargotype.h"
#include "console_func.h"
#include "network/network.h"
#include "network/core/tcp_http.h"
#include <ctype.h>

bool _novahost = false;

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
	CTW_AUTOLOGIN,
	CTW_LOGIN_CREDENTIALS,
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
	CTQ_LOGIN_CREDENTIALS_NAME,
	CTQ_LOGIN_CREDENTIALS_PW,
};

enum CommandsToolbarCargoOption {
	CTW_OPTION_CARGO_AMOUNT = 0,
	CTW_OPTION_CARGO_INCOME,
};
void AccountLogin();
void novaautologin(){
	char msg[128];
	if(_settings_client.network.username == NULL || _settings_client.network.userpw == NULL
		|| strlen(_settings_client.network.username) == 0 || strlen(_settings_client.network.userpw) == 0) return;
	sprintf(msg, "!login %s %s", _settings_client.network.username, _settings_client.network.userpw);
	NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, msg);
}

bool novahost(){
	_novahost = false;
	if(strcmp(_settings_client.network.last_host, "37.157.196.78") == 0
		|| strcmp(_settings_client.network.last_host, "2a02:2b88:2:1::1d73:1") == 0
		|| strcmp(_settings_client.network.last_host, "89.111.65.225") == 0
		|| strcmp(_settings_client.network.last_host, "fe80::20e:7fff:fe23:bee0") == 0
		|| strstr(_settings_client.network.last_host, "novapolis") != NULL)
	{
		_novahost = true;
	}
	return _novahost;
}

/** Commands toolbar window handler. */
struct CommandsToolbarWindow : Window {

	CommandsToolbarQuery query_widget;
	CommandsToolbarCargoOption cargo_option;
	char login_name[25];

	CommandsToolbarWindow(WindowDesc *desc, WindowNumber window_number) : Window(desc)
	{
		this->InitNested(window_number);
		this->cargo_option = CTW_OPTION_CARGO_AMOUNT;
		this->DisableWidget(CTW_CHOOSE_CARGO_INCOME);
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
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!goal");
				break;
			case CTW_SCORE:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!score");
				break;
			case CTW_TOWN:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!town");
				break;
			case CTW_QUEST:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!quest");
				break;
			case CTW_TIMELEFT:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!timeleft");
				break;
			case CTW_INFO:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!info");
				break;
			case CTW_TOWN_ID:
				this->query_widget = CTQ_TOWN_ID;
				ShowQueryString(STR_EMPTY, STR_TOOLBAR_COMMANDS_TOWN_QUERY, 25, this, CS_NUMERAL, QSF_NONE);
				break;
			case CTW_HINT:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!hint");
				break;
			case CTW_LOGIN:
				this->query_widget = CTQ_LOGIN_NAME;
				ShowQueryString(STR_EMPTY, STR_TOOLBAR_COMMANDS_LOGIN_NAME_QUERY, 32, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case CTW_LOGIN_CREDENTIALS:
				this->query_widget = CTQ_LOGIN_CREDENTIALS_NAME;
				ShowQueryString(STR_EMPTY, STR_TOOLBAR_COMMANDS_LOGIN_NAME_QUERY, 32, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case CTW_AUTOLOGIN:
				novaautologin();
				break;
			case CTW_NAME:
				this->query_widget = CTQ_NAME_NEWNAME;
				ShowQueryString(STR_EMPTY, STR_TOOLBAR_COMMANDS_NAME_NEWNAME_QUERY, 25, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case CTW_RESETME:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!resetme");
				break;
			case CTW_SAVEME:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!saveme");
				break;
			case CTW_HELP:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!help");
				break;
			case CTW_RULES:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!rules");
				break;
			case CTW_CBHINT:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!hint");
				break;
			case CTW_CLIENTS:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!clients");
				break;
			case CTW_BEST:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!best");
				break;
			case CTW_ME:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!me");
				break;
			case CTW_RANK:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!rank");
				break;
			case CTW_TOPICS:
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, "!topic");
				break;
			case CTW_TOPIC1:
			case CTW_TOPIC2:
			case CTW_TOPIC3:
			case CTW_TOPIC4:
			case CTW_TOPIC5:
			case CTW_TOPIC6:
				sprintf(msg, "!topic %i", widget - CTW_TOPIC1 + 1);
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, msg);
				break;
			case CTW_CHOOSE_CARGO_AMOUNT:
				this->cargo_option = CTW_OPTION_CARGO_AMOUNT;
				this->DisableWidget(widget);
				this->EnableWidget(CTW_CHOOSE_CARGO_INCOME);
				this->SetDirty();
				break;
			case CTW_CHOOSE_CARGO_INCOME:
				this->cargo_option = CTW_OPTION_CARGO_INCOME;
				this->DisableWidget(widget);
				this->EnableWidget(CTW_CHOOSE_CARGO_AMOUNT);
				this->SetDirty();
				break;
			default:
				if(widget >= CTW_NS0 && widget < CTW_NSEND){
					char ip[16];
					if(widget < CTW_NSX4) strecpy(ip, "37.157.196.78", lastof(ip));
					else strecpy(ip, "89.111.65.225", lastof(ip));

					NetworkClientConnectGame(NetworkAddress(ip, (3980 + widget - CTW_NS0)), COMPANY_SPECTATOR);
				}
				else if (widget >= CTW_CARGO_FIRST) {
					int i = widget - CTW_CARGO_FIRST;
					char name[128];
					GetString(name, _sorted_cargo_specs[i]->name, lastof(name));
					if (this->cargo_option == CTW_OPTION_CARGO_AMOUNT) {
						sprintf(msg, "!A%s", name);
					}
					else {
						sprintf(msg, "!I%s", name);
					}
					NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, msg);
					this->ToggleWidgetLoweredState(widget);
					this->SetDirty();
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
				sprintf(msg, "!town %s", str);
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, msg);
				break;
			case CTQ_LOGIN_NAME:				
				sprintf(msg, "!login %s", str);
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, msg);				
				break;			
			case CTQ_LOGIN_CREDENTIALS_NAME: {
				char name[32];
				char pass[32];
				char buf[64];
				bool space = false;
				//separate name and apss
				for(int i = 0; str[i] != '\0'; i++){
					if(str[i] == ' '){ 
						space = true;
						continue;
					}
					if(!space) name[i] = str[i];
					else pass[i] = str[i];
				}
				
				if(!space) break; //no pass found
				
				uint8 digest[16];
				Md5 checksum;
				checksum.Append(pass, strlen(pass));
				checksum.Finish(digest);
				md5sumToString(buf, lastof(buf), digest);
				for(int i = 0; buf[i] != '\0'; i++){
					buf[i] = tolower(buf[i]);
				}
				strecpy(_settings_client.network.username, this->login_name, lastof(_settings_client.network.username));
				strecpy(_settings_client.network.userpw, buf, lastof(_settings_client.network.userpw));
				SaveToConfig();
				novaautologin();				
				break;
			}
			case CTQ_NAME_NEWNAME:
				this->query_widget = CTQ_NAME_NEWNAME;
				sprintf(msg, "!name %s", str);
				NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, CLIENT_ID_SERVER, msg);
				break;
			default: break;
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
					this->SetDirty();
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
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_GOAL),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_GOAL_CAPTION, STR_TOOLBAR_COMMANDS_GOAL_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_QUEST),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_QUEST_CAPTION, STR_TOOLBAR_COMMANDS_QUEST_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_SCORE),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_SCORE_CAPTION, STR_TOOLBAR_COMMANDS_SCORE_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOWN),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOWN_CAPTION, STR_TOOLBAR_COMMANDS_TOWN_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOWN_ID),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOWN_ID_CAPTION, STR_TOOLBAR_COMMANDS_TOWN_ID_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_HINT),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_HINT_CAPTION, STR_TOOLBAR_COMMANDS_HINT_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_LOGIN),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_LOGIN_CAPTION, STR_TOOLBAR_COMMANDS_LOGIN_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_NAME),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NAME_CAPTION, STR_TOOLBAR_COMMANDS_NAME_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_AUTOLOGIN),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_AUTOLOGIN_CAPTION, STR_TOOLBAR_COMMANDS_AUTOLOGIN_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_LOGIN_CREDENTIALS),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_CREDENTIALS_CAPTION, STR_TOOLBAR_COMMANDS_CREDENTIALS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TIMELEFT),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TIMELEFT_CAPTION, STR_TOOLBAR_COMMANDS_TIMELEFT_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_INFO),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_INFO_CAPTION, STR_TOOLBAR_COMMANDS_INFO_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS1),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS2),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS3),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS4),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS5),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS6),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS7),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NS8),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(0, 1), EndContainer(),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX1),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX2),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX3),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX4),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX5),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX6),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_WHITE, CTW_NSX7),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_NS_CAPTION, STR_TOOLBAR_COMMANDS_NS_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(0, 1), EndContainer(),				
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_RESETME),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_RESETME_CAPTION, STR_TOOLBAR_COMMANDS_RESETME_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_SAVEME),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_SAVEME_CAPTION, STR_TOOLBAR_COMMANDS_SAVEME_TOOLTIP),
			EndContainer(),
			//more
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPICS),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPICS_CAPTION, STR_TOOLBAR_COMMANDS_TOPICS_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_HELP),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_HELP_CAPTION, STR_TOOLBAR_COMMANDS_HELP_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC1),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC1_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC1_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_RULES),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_RULES_CAPTION, STR_TOOLBAR_COMMANDS_RULES_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC2),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC2_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC2_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_CBHINT),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_CBHINT_CAPTION, STR_TOOLBAR_COMMANDS_CBHINT_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC3),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC3_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC3_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_BEST),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_BEST_CAPTION, STR_TOOLBAR_COMMANDS_BEST_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC4),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC4_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC4_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_RANK),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_RANK_CAPTION, STR_TOOLBAR_COMMANDS_RANK_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC5),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC5_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC5_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_ME),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_ME_CAPTION, STR_TOOLBAR_COMMANDS_ME_TOOLTIP),
			EndContainer(),
			NWidget(NWID_HORIZONTAL, NC_EQUALSIZE),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_TOPIC6),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_TOPIC6_CAPTION, STR_TOOLBAR_COMMANDS_TOPIC6_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY), SetResize(0, 1), EndContainer(),
			EndContainer(),
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(0, 1), EndContainer(),
		EndContainer(),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_CHOOSE_CARGO_AMOUNT),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_OPTION_CARGO_A_CAPTION, STR_TOOLBAR_COMMANDS_OPTION_CARGO_A_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, CTW_CHOOSE_CARGO_INCOME),SetMinimalSize(40, 20),SetFill(1, 0), SetDataTip(STR_TOOLBAR_COMMANDS_OPTION_CARGO_I_CAPTION, STR_TOOLBAR_COMMANDS_OPTION_CARGO_I_TOOLTIP),
			EndContainer(),
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

/**
 * Open the commands toolbar window
 *
 * @return newly commands toolbar, or NULL if the toolbar could not be opened.
 */
Window *ShowCommandsToolbar()
{
	DeleteWindowByClass(WC_COMMAND_TOOLBAR);
	return AllocateWindowDescFront<CommandsToolbarWindow>(&_commands_toolbar_desc, 0);
}

#endif  /* ENABLE_NETWORK */
