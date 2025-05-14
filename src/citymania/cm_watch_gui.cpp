/** @file watch_gui.cpp GUI that follow other company building. */

#include "../stdafx.h"

#include "cm_watch_gui.hpp"

#include "cm_hotkeys.hpp"
#include "cm_main.hpp"

#include "../widget_type.h"
#include "../gfx_type.h"
#include "../gfx_func.h"
#include "../gui.h"
#include "../company_base.h"
#include "../company_gui.h"
#include "../landscape.h"
#include "../map_func.h"
#include "../strings_func.h"
#include "../viewport_func.h"
#include "../window_func.h"
#include "../zoom_func.h"

#include "../network/network.h"
#include "../network/network_func.h"
#include "../network/network_base.h"
#include "../network/network_gui.h"
#include "../table/sprites.h"
#include "../table/strings.h"

#include "../textbuf_gui.h"
#include "../company_gui.h" //company window
#include "../network/network_gui.h" //private message
#include "../console_func.h" //IConsolePrintF
#include "../debug.h"

namespace citymania {

/** Make the widgets columns for company button, has_client and activity Blot.
 * @param biggest_index Storage for collecting the biggest index used in the returned tree.
 * @return Horizontal container with butons columns.
 * @post \c *biggest_index contains the largest used index in the tree.
 */
// static static std::unique_ptr<NWidgetBase> MakeCompanyButtons()
// {
// 	NWidgetHorizontal *widget_container_horiz = NULL;
// 	NWidgetVertical *widget_container_company = NULL;
// 	NWidgetVertical *widget_container_hasclient = NULL;

// 	auto widget_container_horiz = std::make_unique<NWidgetHorizontal>();  // Storage for all cols.
// 	auto widget_container_company = std::make_unique<NWidgetVertical>();  // Storage for company Col.
// 	auto widget_container_hasclient  = std::make_unique<NWidgetVertical>();  // Storage for Has Client Blot.
// 	// NWidgetVertical *widget_container_activity = NULL;        // Storage for Activity Blot.
// 	// widget_container_activity = new NWidgetVertical( );

// 	Dimension company_sprite_size = GetSpriteSize( SPR_COMPANY_ICON );
// 	company_sprite_size.width  += WidgetDimensions::scaled.matrix.Horizontal();
// 	company_sprite_size.height += WidgetDimensions::scaled.matrix.Vertical() + 1; // 1 for the 'offset' of being pressed

// 	Dimension blot_sprite_size = GetSpriteSize( SPR_BLOT );
// 	blot_sprite_size.width  += WidgetDimensions::scaled.matrix.Horizontal();
// 	blot_sprite_size.height += WidgetDimensions::scaled.matrix.Vertical() + 1; // 1 for the 'offset' of being pressed


// 	for (int company_num = COMPANY_FIRST; company_num < MAX_COMPANIES; company_num++ ) {
// 		/* Manage Company Buttons */
// 		auto company_panel = std::make_unique<NWidgetBackground>( WWT_PANEL, COLOUR_GREY, EWW_PB_COMPANY_FIRST + company_num );
// 		company_panel->SetMinimalSizeAbsolute(company_sprite_size.width, company_sprite_size.height);
// 		company_panel->SetResize( 0, 0 );
// 		company_panel->SetFill( 1, 0 );
// 		company_panel->SetDataTip( 0x0, CM_STR_WATCH_CLICK_TO_WATCH_COMPANY );
// 		widget_container_company->Add(std::move(company_panel));

// 		/* Manage Has Client Blot */

// 		auto hasclient_panel = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_GREY, EWW_HAS_CLIENT_FIRST + company_num);
// 		company_panel->SetMinimalSizeAbsolute( blot_sprite_size.width, blot_sprite_size.height );
// 		company_panel->SetResize( 0, 0 );
// 		company_panel->SetFill( 1, 0 );
// 		widget_container_hasclient->Add(std::move(hasclient_panel));
// 	}

// 	/* Add the verticals widgets to the horizontal container */
// 	widget_container_horiz->Add(std::move(widget_container_company));
// 	widget_container_horiz->Add(std::move(widget_container_hasclient));

// 	/* return the horizontal widget container */
// 	return widget_container_horiz;
// }

static std::unique_ptr<NWidgetBase> MakeCompanyButtons2() {
	auto widget_container_horiz = std::make_unique<NWidgetHorizontal>();

	Dimension company_sprite_size = GetSpriteSize(CM_SPR_COMPANY_ICON);
	company_sprite_size.width  += WidgetDimensions::scaled.matrix.Horizontal();
	company_sprite_size.height += WidgetDimensions::scaled.matrix.Vertical() + 1; // 1 for the 'offset' of being pressed

	// for (const Company *c : Company::Iterate()) {
	for (int widnum = EWW_COMPANY_BEGIN; widnum < EWW_COMPANY_END; widnum++) {
		auto company_panel = std::make_unique<NWidgetBackground>(WWT_PANEL, COLOUR_GREY, widnum);
		company_panel->SetMinimalSizeAbsolute(company_sprite_size.width, company_sprite_size.height);
		company_panel->SetResize(1, 0);
		company_panel->SetFill(1, 1);
		company_panel->SetDataTip(0, CM_STR_WATCH_CLICK_TO_WATCH_COMPANY);
		widget_container_horiz->Add(std::move(company_panel));
	}

	// auto filler = new NWidgetBackground(WWT_PANEL, COLOUR_GREY, EWW_COMPANY_FILLER);
	// filler->SetFill(1, 1);
	// filler->SetResize(1, 0);
	// filler->SetMinimalSizeAbsolute(0, company_sprite_size.height);
	// filler->SetResize(0, 0);
	// filler->SetDataTip(0, 0);
	// widget_container_horiz->Add(filler);

	return widget_container_horiz;
}

/**
 * Watch Company Window Widgets Array
 * The Company Button, Has Client Blot and Activity Blot Columns
 * Are made through a function regarding MAX_COMPANIES value
 */
static const NWidgetPart _nested_watch_company_widgets[] = {
	/* Title Bar with close box, title, shade and stick boxes */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, EWW_CAPTION ), SetDataTip(CM_STR_WATCH_WINDOW_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, EWW_LOCATION), SetMinimalSize(12, 14), SetDataTip(SPR_GOTO_LOCATION, CM_STR_WATCH_LOCATION_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer( ),
	// NWidget( NWID_HORIZONTAL ),
	// 	NWidget( NWID_VERTICAL ),
	// 		NWidgetFunction( MakeCompanyButtons ),
	// 		/* Buton Zoom Out, In, Scrollto */
	// 		NWidget(NWID_HORIZONTAL),
	// 			NWidget( WWT_PUSHIMGBTN, COLOUR_GREY, EWW_ZOOMOUT ), SetDataTip( SPR_IMG_ZOOMOUT, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_OUT),
	// 			NWidget( WWT_PUSHIMGBTN, COLOUR_GREY, EWW_ZOOMIN ),  SetDataTip( SPR_IMG_ZOOMIN, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_IN),
	// 			NWidget( WWT_PUSHIMGBTN, COLOUR_GREY, EWW_CENTER ),  SetDataTip( SPR_CENTRE_VIEW_VEHICLE, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW_TT),
	// 			NWidget( WWT_PANEL, COLOUR_GREY, EWW_NEW_WINDOW ),   SetDataTip( 0, STR_WATCH_CLICK_NEW_WINDOW ), EndContainer( ),
	// 		EndContainer( ),
	// 		/* Background panel for resize purpose */
	// 		NWidget( WWT_PANEL, COLOUR_GREY ), SetResize( 0, 1 ), EndContainer( ),
	// 	EndContainer( ),
	// 	/* Watch Pannel */
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, EWW_WATCH), SetPadding(2, 2, 2, 2), SetResize(1, 1), SetFill(1, 1),
		EndContainer( ),
	// EndContainer( ),
	/* Status Bar with resize buton */
	NWidget(NWID_HORIZONTAL),
		// NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		NWidgetFunction(MakeCompanyButtons2),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer( ),
};

/**
 * Watch Company Window Descriptor
 */
static WindowDesc _watch_company_desc(
	WDP_AUTO, "cm_watch_gui", 300, 257,
	WC_WATCH_COMPANY, WC_NONE,
	WDF_NO_FOCUS,
	_nested_watch_company_widgets
);

// admin version
static const NWidgetPart _nested_watch_company_widgetsA[] = {
	/* Title Bar with close box, title, shade and stick boxes */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, EWW_CAPTION ), SetDataTip(CM_STR_WATCH_WINDOW_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, EWW_LOCATION), SetMinimalSize(12, 14), SetDataTip(SPR_GOTO_LOCATION, CM_STR_WATCH_LOCATION_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget( NWID_HORIZONTAL ),
		NWidget( NWID_VERTICAL ),
			/* Buton Zoom Out, In, Scrollto */
		NWidget(NWID_SELECTION, INVALID_COLOUR, EWW_ENABLE_SELECT),
				NWidget(NWID_VERTICAL ),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, EWW_KICK), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(CM_STR_XI_KICK, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, EWW_BAN), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(CM_STR_XI_BAN, 0 ),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, EWW_LOCK), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(CM_STR_XI_LOCK, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, EWW_UNLOCK), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(CM_STR_XI_UNLOCK, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, EWW_JOIN), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(CM_STR_XI_JOIN, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, EWW_KICKC), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(CM_STR_XI_KICKC, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, EWW_RESET), SetMinimalSize(40, 20), SetFill(1, 0), SetDataTip(CM_STR_XI_RESET, 0),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, EWW_ZOOMOUT), SetDataTip(SPR_IMG_ZOOMOUT, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_OUT),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, EWW_ZOOMIN), SetDataTip(SPR_IMG_ZOOMIN, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_IN),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, EWW_CENTER), SetDataTip(SPR_CENTRE_VIEW_VEHICLE, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW_TT),
				NWidget(WWT_PANEL, COLOUR_GREY, EWW_NEW_WINDOW), SetDataTip(0, 0), EndContainer(),
			EndContainer(),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, EWW_CLIENTS), SetMinimalSize(23, 10), SetDataTip(SPR_IMG_COMPANY_GENERAL, CM_STR_XI_PLAYERS_TOOLTIP),
				NWidget(WWT_PUSHIMGBTN, COLOUR_GREY, EWW_COMPANYW), SetMinimalSize(23, 10), SetDataTip(SPR_IMG_COMPANY_LIST, CM_STR_XI_COMPANYW_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, EWW_COMPANYHQ), SetMinimalSize(23, 10), SetDataTip(CM_STR_XI_COMPANYHQ, CM_STR_XI_COMPANYHQ_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, EWW_PRIVATEP_MESSAGE), SetMinimalSize(23, 10), SetDataTip(CM_STR_XI_PRIVATE_PLAYER_MESSAGE, CM_STR_XI_PRIVATE_PLAYER_MESSAGE_TOOLTIP),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, EWW_PRIVATEC_MESSAGE), SetMinimalSize(23, 10), SetDataTip(CM_STR_XI_PRIVATE_COMPANY_MESSAGE, CM_STR_XI_PRIVATE_COMPANY_MESSAGE_TOOLTIP),
				NWidget(WWT_PANEL, COLOUR_GREY, EWW_NEW_WINDOW), SetDataTip(0, 0), EndContainer(),
			EndContainer(),
			/* Background panel for resize purpose */
			NWidget(WWT_PANEL, COLOUR_GREY), SetResize(0, 1), EndContainer(),
		EndContainer(),
		/* Watch Pannel */
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, EWW_WATCH), SetPadding(2, 2, 2, 2), SetResize(1, 1), SetFill(1, 1),
		EndContainer(),
	EndContainer(),
	/* Status Bar with resize buton */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer(),
};

/**
 * Watch Company Window Descriptor
 */
static WindowDesc _watch_company_descA(
	WDP_AUTO, "watch_gui_client", 448, 256,
	WC_WATCH_COMPANYA, WC_NONE,
	WDF_CONSTRUCTION,
	_nested_watch_company_widgetsA
);

static void ResetCallback(Window *w, bool confirmed)
{
	if (confirmed) {
		NetworkClientInfo *ci = NetworkClientInfo::GetByClientID((ClientID)w->window_number);
		if (ci && ci->client_playas != INVALID_COMPANY) {
			NetworkClientSendChatToServer(fmt::format("!reset {}", ci->client_playas + 1));
		}
	}
}

/** Watch Company Class Constructor
 * @param desc Window Descriptor The Window Descriptor
 * @param window_number The window number for the class
 * @param company_to_watch Company ID for watching a particular company
 */
WatchCompany::WatchCompany(WindowDesc &desc, int window_number, CompanyID company_to_watch = INVALID_COMPANY, int Wtype = EWT_COMPANY) : Window(desc)
{
	this->Wtype = Wtype;
	if(this->Wtype == EWT_CLIENT){
		this->watched_client = window_number;
		NetworkClientInfo *ci = NetworkClientInfo::GetByClientID((ClientID)this->watched_client);

		this->watched_company = (ci != NULL) ? ci->client_playas : INVALID_COMPANY;
		this->InitNested(window_number);
		this->owner = (Owner)this->watched_company;
	}
	else if(this->Wtype == EWT_COMPANY){

		this->watched_company = company_to_watch;

		this->InitNested(window_number);
		this->owner = this->watched_company;

		/* Reset activity and client count for all companies */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->company_activity[i] = 0;
			this->company_count_client[i] = 0;
		}

		this->company_name = GetString(STR_JUST_NOTHING);
	}
	/* Init the viewport area */
	NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(EWW_WATCH);
	nvp->InitializeViewport(this, 0, ZOOM_LVL_NORMAL);

	Point pt;
	/* the main window with the main view */
	const Window *w = FindWindowById(WC_MAIN_WINDOW, 0);

	/* center on same place as main window (zoom is maximum, no adjustment needed) */
	pt.x = w->viewport->scrollpos_x + w->viewport->virtual_width / 2;
	pt.y = w->viewport->scrollpos_y + w->viewport->virtual_height / 2;

	this->viewport->scrollpos_x = pt.x - this->viewport->virtual_width / 2;
	this->viewport->scrollpos_y = pt.y - this->viewport->virtual_height / 2;
	this->viewport->dest_scrollpos_x = this->viewport->scrollpos_x;
	this->viewport->dest_scrollpos_y = this->viewport->scrollpos_y;

	if ( this->watched_company != INVALID_COMPANY ) {
		Company *c = Company::Get( this->watched_company );
		this->ScrollToTile( c->last_build_coordinate );
	}
	this->InvalidateData( );
}

void WatchCompany::SetStringParameters(int widget) const
{
	if(widget != EWW_CAPTION) return;
	if(this->Wtype == EWT_COMPANY){
		SetDParamStr(0, this->company_name);
		return;
	}
	//EWT_CLIENT
	if (!Company::IsValidHumanID(this->watched_company)){
		// GetString((char *)this->company_name, STR_JUST_NOTHING, lastof(this->company_name));
	}
	else {
		const Company *c = Company::Get(this->watched_company);
		SetDParam(0, c->index);
		// GetString((char *)this->company_name, STR_COMPANY_NAME, lastof(this->company_name));
	}
	NetworkClientInfo *ci = NetworkClientInfo::GetByClientID((ClientID)this->watched_client);
	if(ci){
		// strecpy((char *)this->client_name, ci->client_name, lastof(this->client_name));
	}
	else{
		// GetString((char *)this->client_name, STR_JUST_NOTHING, lastof(this->client_name));
	}
	SetDParamStr(0, this->client_name);
	SetDParamStr(1, this->company_name);
	if (Company::IsValidHumanID(this->watched_company)){
		SetDParam(2, this->watched_company + 1);
	}
	else {
		SetDParam(2, COMPANY_SPECTATOR);
	}
}

void WatchCompany::OnPaint()
{
	if(this->Wtype == EWT_CLIENT){
		bool wstate = this->watched_company == INVALID_COMPANY ? true : false;
		for(int i = EWW_LOCK; i <= EWW_COMPANYW; i++){
			this->SetWidgetDisabledState(i, wstate);
		}
	}

	this->DrawWidgets();
}

void WatchCompany::DrawWidget(const Rect &r, int widget) const
{
	if(this->Wtype != EWT_COMPANY) return;
	/* draw the widget */
	/* Company Button */
	if (IsInsideMM(widget, EWW_PB_COMPANY_FIRST, EWW_PB_COMPANY_LAST + 1)) {
		if (this->IsWidgetDisabled(widget)) return;
		if ( Company::IsValidID( widget - EWW_PB_COMPANY_FIRST ) ) {
		    CompanyID cid = (CompanyID)(widget - ( EWW_PB_COMPANY_FIRST ) );
		    int offset = (cid == this->watched_company) ? 1 : 0;
		    Dimension sprite_size = GetSpriteSize(SPR_COMPANY_ICON);
		    DrawCompanyIcon(cid, (r.left + r.right - sprite_size.width) / 2 + offset, (r.top + r.bottom - sprite_size.height) / 2 + offset);
		}
		return;
	}
	if (IsInsideMM(widget, EWW_COMPANY_BEGIN, EWW_COMPANY_END)) {
		if (this->IsWidgetDisabled(widget)) return;
		CompanyID cid = (CompanyID)(widget - EWW_COMPANY_BEGIN);
		if (Company::IsValidID(cid)) {
		    int offset = (cid == this->watched_company) ? 1 : 0;
			auto icon = (company_activity[cid] > 0 ? CM_SPR_COMPANY_ICON : CM_SPR_COMPANY_ICON_AFK);
		    Dimension sprite_size = GetSpriteSize(icon);
			DrawSprite(icon, COMPANY_SPRITE_COLOUR(cid), (r.left + r.right - sprite_size.width) / 2 + offset, (r.top + r.bottom - sprite_size.height) / 2 + offset);
		}
		return;
	}
	/* Has Client Blot */
	if (IsInsideMM( widget, EWW_HAS_CLIENT_FIRST, EWW_HAS_CLIENT_LAST + 1 )) {
		if ( Company::IsValidID( widget-EWW_HAS_CLIENT_FIRST ) ) {
			/* Draw the Blot only if Company Exists */
			Dimension sprite_size = GetSpriteSize(SPR_BLOT);
			if (!_networking) { // Local game, draw the Blot
				DrawSprite(SPR_BLOT, Company::IsValidAiID(widget - EWW_HAS_CLIENT_FIRST) ? PALETTE_TO_ORANGE : PALETTE_TO_GREEN, (r.left + r.right - sprite_size.width) / 2, (r.top + r.bottom - sprite_size.height) / 2 );
			} else { // Network game, draw the blot according to company client count
				DrawSprite(SPR_BLOT, this->company_count_client[widget-EWW_HAS_CLIENT_FIRST] > 0 ? (company_activity[widget-EWW_HAS_CLIENT_FIRST] > 0 ? PALETTE_TO_RED : PALETTE_TO_GREEN) : PALETTE_TO_GREY, (r.left + r.right - sprite_size.width) / 2, (r.top + r.bottom - sprite_size.height) / 2 );
			}
		}
	}
}

void WatchCompany::OnResize()
{
	if (this->viewport != NULL) {
		NWidgetViewport *nvp = this->GetWidget<NWidgetViewport>(EWW_WATCH);
		nvp->UpdateViewportCoordinates(this);
	}
}

void WatchCompany::OnScroll(Point delta)
{
	const Viewport *vp = IsPtInWindowViewport(this, _cursor.pos.x, _cursor.pos.y);
	if (vp == NULL) return;

	this->viewport->scrollpos_x += ScaleByZoom(delta.x, vp->zoom);
	this->viewport->scrollpos_y += ScaleByZoom(delta.y, vp->zoom);
	this->viewport->dest_scrollpos_x = this->viewport->scrollpos_x;
	this->viewport->dest_scrollpos_y = this->viewport->scrollpos_y;
}

void WatchCompany::OnMouseWheel( int wheel )
{
	ZoomInOrOutToCursorWindow(wheel < 0, this);
}

void WatchCompany::OnClick(Point /* pt */, int widget, int /* click_count */)
{
	/* Check which button is clicked */
	if (IsInsideMM(widget, EWW_COMPANY_BEGIN, EWW_COMPANY_END)) {
		/* Is it no on disable? */
		if (!this->IsWidgetDisabled(widget)) {
			if (this->watched_company != INVALID_COMPANY)
				this->RaiseWidget(EWW_COMPANY_BEGIN + this->watched_company);
			auto c = Company::GetIfValid((CompanyID)(widget - EWW_COMPANY_BEGIN));
			if (c == nullptr || this->watched_company == c->index) {
				this->watched_company = INVALID_COMPANY;
			} else {
				this->watched_company = c->index;
				this->LowerWidget(widget);
				SetDParam(0, c->index);
				this->company_name = GetString(STR_COMPANY_NAME);
				this->ScrollToTile(c->last_build_coordinate);
			}
			this->owner = this->watched_company;
			this->SetDirty();
		}
	} else if (widget == EWW_LOCATION) {
		auto x = this->viewport->scrollpos_x + this->viewport->virtual_width / 2;
		auto y = this->viewport->scrollpos_y + this->viewport->virtual_height / 2;
		auto pt = InverseRemapCoords(x, y);
		if (_fn_mod) citymania::ShowExtraViewportWindow(pt.x, pt.y, 0);
		else ScrollMainWindowTo(pt.x, pt.y, 0);
	} else if (IsInsideMM(widget, EWW_PB_COMPANY_FIRST, EWW_PB_COMPANY_LAST + 1)) {
		/* Click on Company Button */
		if (!this->IsWidgetDisabled(widget)) {
			if (this->watched_company != INVALID_COMPANY) {
				/* Raise the watched company button  */
				this->RaiseWidget(EWW_PB_COMPANY_FIRST + this->watched_company);
			}
			if (this->watched_company == (CompanyID)(widget - EWW_PB_COMPANY_FIRST)) {
				/* Stop watching watched_company */
				this->watched_company = INVALID_COMPANY;
				this->company_name = GetString(STR_JUST_NOTHING);
			} else {
				/* Lower the new watched company button */
				this->watched_company = (CompanyID)(widget - EWW_PB_COMPANY_FIRST);
				this->LowerWidget( EWW_PB_COMPANY_FIRST + this->watched_company);
				Company *c = Company::Get( this->watched_company );
				SetDParam( 0, c->index );
				this->company_name = GetString(STR_COMPANY_NAME);

				this->ScrollToTile( c->last_build_coordinate );
			}
			this->owner = this->watched_company;
			this->SetDirty();
		}
	}
	else if ( IsInsideMM(widget, EWW_PB_ACTION1_FIRST, EWW_PB_ACTION1_LAST + 1)) {
		if ( !this->IsWidgetDisabled(widget) ) {
			this->ToggleWidgetLoweredState( widget );
			this->SetDirty();
		}
	}
	else if ( IsInsideMM(widget, EWW_HAS_CLIENT_FIRST, EWW_HAS_CLIENT_LAST + 1)) {
		if(_networking && Company::IsValidID(widget - EWW_HAS_CLIENT_FIRST)){
			ShowNetworkChatQueryWindow(DESTTYPE_TEAM, widget - EWW_HAS_CLIENT_FIRST);
		}
	}
	else {
		switch (widget) {
			case EWW_ZOOMOUT: DoZoomInOutWindow(ZOOM_OUT, this); break;
			case EWW_ZOOMIN: DoZoomInOutWindow(ZOOM_IN,  this); break;
			case EWW_CENTER: { // location button (move main view to same spot as this view) 'Center Main View'
				Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
				int x = this->viewport->scrollpos_x; // Where is the watch looking at
				int y = this->viewport->scrollpos_y;

				/* set the main view to same location. Based on the center, adjusting for zoom */
				w->viewport->dest_scrollpos_x =  x - (w->viewport->virtual_width -  this->viewport->virtual_width) / 2;
				w->viewport->dest_scrollpos_y =  y - (w->viewport->virtual_height - this->viewport->virtual_height) / 2;
			} break;
			case EWW_NEW_WINDOW:
				ShowWatchWindow(this->watched_company, 0);
				break;
			case EWW_LOCK:
			case EWW_UNLOCK:
				NetworkClientSendChatToServer(fmt::format("!lockp {}", this->watched_company + 1));
				break;
			case EWW_KICK:
				NetworkClientSendChatToServer(fmt::format("!kick {}", this->watched_client));
				break;
			case EWW_KICKC:
				NetworkClientSendChatToServer(fmt::format("!move {} {}", this->watched_client, COMPANY_SPECTATOR));
				break;
			case EWW_JOIN:
				NetworkClientSendChatToServer(fmt::format("!move {}", this->watched_company + 1));
				break;
			case EWW_BAN:
				this->query_widget = EWQ_BAN;
				ShowQueryString(CM_STR_XI_BAN_DAYSDEFAULT, CM_STR_XI_BAN_QUERY, 128, this, CS_ALPHANUMERAL, QSF_NONE);
				break;
			case EWW_RESET:
				ShowQuery(CM_STR_XI_RESET_CAPTION, CM_STR_XI_REALY_RESET, this, ResetCallback);
				break;
			case EWW_PRIVATEP_MESSAGE:{
				const NetworkClientInfo *ci = NetworkClientInfo::GetByClientID((ClientID)this->watched_client);
				if(ci) ShowNetworkChatQueryWindow(DESTTYPE_CLIENT, ci->client_id);
				break;
			}
			case EWW_PRIVATEC_MESSAGE:
				ShowNetworkChatQueryWindow(DESTTYPE_TEAM, this->watched_company);
				break;
			case EWW_CLIENTS:
				NetworkClientSendChatToServer("!clients");
				break;
			case EWW_COMPANYW:
				if(this->watched_company != INVALID_COMPANY) ShowCompany(this->watched_company);
				break;
			case EWW_COMPANYHQ:
				if(this->watched_company != INVALID_COMPANY){
					TileIndex tile = Company::Get((CompanyID)this->watched_company)->location_of_HQ;
					ScrollMainWindowToTile(tile);
				}
				break;
		}
	}
}

void WatchCompany::OnQueryTextFinished(std::optional<std::string> str)
{
	if (!str.has_value()) return;
	switch (this->query_widget) {
		case EWQ_BAN:
			NetworkClientSendChatToServer(fmt::format("!ban {} {}", this->watched_client, *str));
			break;
		default:
			break;
	}
}

void WatchCompany::OnInvalidateData(int data, bool /* gui_scope */)
{
	if(this->Wtype == EWT_COMPANY){
		/* Disable the companies who are not active */
		// for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		// 	this->SetWidgetDisabledState(EWW_PB_COMPANY_FIRST + i , !Company::IsValidID(i) );
		// 	this->SetWidgetDisabledState(EWW_PB_ACTION1_FIRST + i , !Company::IsValidID(i) );
		// }
		// /* Check if the currently selected company is still active. */
		// if (this->watched_company != INVALID_COMPANY) {
		// 	/* Make sure the widget is lowered */
		// 	this->LowerWidget(EWW_PB_COMPANY_FIRST + this->watched_company);
		// 	/* Check if the watched Company is still a valid one */
		// 	if (!Company::IsValidID(this->watched_company)) {
		// 		/* Invalid Company Raise the associated widget. */
		// 		this->RaiseWidget(this->watched_company + EWW_PB_COMPANY_FIRST );
		// 		this->watched_company = INVALID_COMPANY;
		// 		GetString( this->company_name, STR_JUST_NOTHING, lastof(this->company_name) );
		// 	} else {
		// 		Company *c = Company::Get( this->watched_company );
		// 		SetDParam( 0, c->index );
		// 		GetString( this->company_name, STR_COMPANY_NAME, lastof(this->company_name) );
		// 	}
		// } else {
		// 	GetString( this->company_name, STR_JUST_NOTHING, lastof(this->company_name) );

		// }
		// if (_networking) { // Local game, draw the Blot
		// 	/* Reset company count - network only */
		// 	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		// 		this->company_count_client[i] = 0;
		// 	}
		// 	/* Calculate client count into company - network only */
		// 	for (NetworkClientInfo *ci : NetworkClientInfo::Iterate()) {
		// 		if (Company::IsValidID(ci->client_playas)) {
		// 			company_count_client[ci->client_playas] += 1;
		// 		}
		// 	}
		// }

		/* Disable the companies who are not active */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->SetWidgetDisabledState(EWW_COMPANY_BEGIN + i, !Company::IsValidID(i));
		}

		/* Check if the currently selected company is still active. */
		if (this->watched_company != INVALID_COMPANY && !Company::IsValidID(this->watched_company)) {
			/* Raise the widget for the previous selection. */
			this->RaiseWidget(EWW_COMPANY_BEGIN + this->watched_company);
			this->watched_company = INVALID_COMPANY;
		}

		// if (this->company == INVALID_COMPANY) {
		// 	for (const Company *c : Company::Iterate()) {
		// 		this->company = c->index;
		// 		break;
		// 	}
		// }

		/* Make sure the widget is lowered */
		if (this->watched_company != INVALID_COMPANY)
			this->LowerWidget(EWW_COMPANY_BEGIN + this->watched_company);
	}
	else if(this->Wtype == EWT_CLIENT){
		if (data == 2) {
			this->Close();
			return;
		}
		NetworkClientInfo *ci = NetworkClientInfo::GetByClientID((ClientID)this->watched_client);
		if (!ci) {
			this->Close();
			return;
		}
		else {
			this->watched_company = ci->client_playas;
			this->owner = (Owner)this->watched_company;
			bool wstate = this->watched_company == INVALID_COMPANY ? true : false;
			for(int i = EWW_LOCK; i <= EWW_COMPANYW; i++){
				this->SetWidgetDisabledState(i, wstate);
			}
		}
	}
	// HandleZoomMessage(this, this->viewport, EWW_ZOOMIN, EWW_ZOOMOUT);
}

void WatchCompany::ScrollToTile( TileIndex tile )
{
	/* Scroll window to the tile, only if not zero */
	if (tile != 0) {
		ScrollWindowTo(TileX(tile) * TILE_SIZE + TILE_SIZE / 2, TileY(tile) * TILE_SIZE + TILE_SIZE / 2, -1, this);
	}
}

/** OnDoCommand function - Called by the DoCommand
 *  @param company The company ID who's client is building
 *  @param tile The tile number where action took place
 */
void WatchCompany::OnDoCommand(CompanyID company, TileIndex tile )
{
	/* Check if its my company */
	if (this->watched_company == company)
	{
		this->ScrollToTile(tile);
	}
	/* set the company_activity to its max in order to paint the BLOT in red
	 * This will result by having the activity blot set to red for all companies
	 * even the one watched. To avoid this behaviour and not to light the blot of
	 * the watched company, the code can be moved just after the ScrollToTile call.
	 */
	if (tile != 0) {
		this->company_activity[company] = MAX_ACTIVITY;
		this->SetDirty( );
	}
}

/** Used to decrement the activity counter
 *
 */
void WatchCompany::OnRealtimeTick([[maybe_unused]] uint delta_ms)
{
	bool set_dirty = false;
	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		if (this->company_activity[i] > 0) {
			this->company_activity[i]--;
			if (this->company_activity[i] == 0) {
				set_dirty = true;
			}
		}
	}
	/* If one company_activity reaches 0, then redraw */
	if (set_dirty) {
		this->SetDirty();
	}
}

void ShowWatchWindow(CompanyID company_to_watch = INVALID_COMPANY, int type = EWT_COMPANY)
{
	if(type == EWT_COMPANY) {
		int i = 0;
		/* find next free window number for watch viewport */
		while (FindWindowById(WC_WATCH_COMPANY, i) != NULL) i++;
		new WatchCompany(_watch_company_desc, i, company_to_watch, type);
	} else if(type == EWT_CLIENT) {
		if (BringWindowToFrontById(WC_WATCH_COMPANYA, company_to_watch)) return;
		new WatchCompany(_watch_company_descA, company_to_watch, company_to_watch, type);
	}
}

void UpdateWatching(CompanyID /* company */, TileIndex tile) {
	/* Send Tile Number to Watching Company Windows */
	WatchCompany *wc;
	for(int watching_window = 0; ; watching_window++){
		wc = dynamic_cast<citymania::WatchCompany*>(FindWindowById(WC_WATCH_COMPANY, watching_window));
		if(wc != NULL) wc->OnDoCommand(_current_company, tile);
		else break;
	}

	for (const NetworkClientInfo *ci : NetworkClientInfo::Iterate()) {
		if (ci->client_playas == _current_company) {
			wc = dynamic_cast<citymania::WatchCompany*>(FindWindowById(WC_WATCH_COMPANYA, ci->client_id));
			if (wc != NULL) wc->OnDoCommand(_current_company, tile);
			break;
		}
	}
}

} // namespace citymania
