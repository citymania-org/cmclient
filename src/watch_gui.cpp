/* $Id: watch_gui.cpp  17678 2009-10-07 20:54:05  muxy $ */

/** @file watch_gui.cpp GUI that follow other company building. */

#include "stdafx.h"
#include "watch_gui.h"
#include "widget_type.h"
#include "gfx_type.h"
#include "gfx_func.h"
#include "company_base.h"
#include "company_gui.h"
#include "viewport_func.h"
#include "window_func.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "map_func.h"

#include "network/network.h"
#include "network/network_func.h"
#include "network/network_base.h"
#include "network/network_gui.h"
#include "table/sprites.h"
#include "table/strings.h"

/** Make the widgets columns for company button, has_client and activity Blot.
 * @param biggest_index Storage for collecting the biggest index used in the returned tree.
 * @return Horizontal container with butons columns.
 * @post \c *biggest_index contains the largest used index in the tree.
 */
static NWidgetBase *MakeCompanyButtons(int *biggest_index)
{
	NWidgetHorizontal *widget_container_horiz = NULL;         // Storage for all cols.
	NWidgetVertical *widget_container_company = NULL;         // Storage for company Col.
	NWidgetVertical *widget_container_hasclient = NULL;       // Storage for Has Client Blot.
	NWidgetVertical *widget_container_activity = NULL;        // Storage for Activity Blot.

	widget_container_horiz = new NWidgetHorizontal( );
	widget_container_company = new NWidgetVertical( );
	widget_container_hasclient  = new NWidgetVertical( );
	widget_container_activity = new NWidgetVertical( );
	
	Dimension company_sprite_size = GetSpriteSize( SPR_COMPANY_ICON );
	company_sprite_size.width  += WD_MATRIX_LEFT + WD_MATRIX_RIGHT;
	company_sprite_size.height += WD_MATRIX_TOP + WD_MATRIX_BOTTOM + 1; // 1 for the 'offset' of being pressed

	Dimension blot_sprite_size = GetSpriteSize( SPR_BLOT );
	blot_sprite_size.width  += WD_MATRIX_LEFT + WD_MATRIX_RIGHT;
	blot_sprite_size.height += WD_MATRIX_TOP + WD_MATRIX_BOTTOM + 1; // 1 for the 'offset' of being pressed


	for (int company_num = COMPANY_FIRST; company_num < MAX_COMPANIES; company_num++ ) {
		/* Manage Company Buttons */
		NWidgetBackground *company_panel = new NWidgetBackground( WWT_PANEL, COLOUR_GREY, EWW_PB_COMPANY_FIRST + company_num );
		company_panel->SetMinimalSize( company_sprite_size.width, company_sprite_size.height );
		company_panel->SetResize( 0, 0 );
		company_panel->SetFill( 1, 0 );
		company_panel->SetDataTip( 0x0, STR_WATCH_CLICK_TO_WATCH_COMPANY );
		widget_container_company->Add( company_panel );

		/* Manage Has Client Blot */
		NWidgetBackground *hasclient_panel = new NWidgetBackground( WWT_PANEL, COLOUR_GREY, EWW_HAS_CLIENT_FIRST + company_num );
		company_panel->SetMinimalSize( blot_sprite_size.width, blot_sprite_size.height );
		company_panel->SetResize( 0, 0 );
		company_panel->SetFill( 1, 0 );
		widget_container_hasclient->Add( hasclient_panel );
	}

	/* Add the verticals widgets to the horizontal container */
	widget_container_horiz->Add( widget_container_company );
	widget_container_horiz->Add( widget_container_hasclient );

	/* return the horizontal widget container */
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
		NWidget(WWT_CAPTION, COLOUR_GREY, EWW_CAPTION ), SetDataTip(STR_WATCH_WINDOW_TITLE, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer( ),
	NWidget( NWID_HORIZONTAL ),
		NWidget( NWID_VERTICAL ),
			NWidgetFunction( MakeCompanyButtons ),
			/* Buton Zoom Out, In, Scrollto */
			NWidget(NWID_HORIZONTAL),
				NWidget( WWT_PUSHIMGBTN, COLOUR_GREY, EWW_ZOOMOUT ), SetDataTip( SPR_IMG_ZOOMOUT, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_OUT),
				NWidget( WWT_PUSHIMGBTN, COLOUR_GREY, EWW_ZOOMIN ),  SetDataTip( SPR_IMG_ZOOMIN, STR_TOOLBAR_TOOLTIP_ZOOM_THE_VIEW_IN),
				NWidget( WWT_PUSHIMGBTN, COLOUR_GREY, EWW_CENTER ),  SetDataTip( SPR_CENTRE_VIEW_VEHICLE, STR_EXTRA_VIEW_MOVE_MAIN_TO_VIEW_TT),
				NWidget( WWT_PANEL, COLOUR_GREY, EWW_NEW_WINDOW ),   SetDataTip( 0, STR_WATCH_CLICK_NEW_WINDOW ), EndContainer( ),
			EndContainer( ),
			/* Background panel for resize purpose */
			NWidget( WWT_PANEL, COLOUR_GREY ), SetResize( 0, 1 ), EndContainer( ),
		EndContainer( ),
		/* Watch Pannel */
		NWidget(WWT_PANEL, COLOUR_GREY),
			NWidget(NWID_VIEWPORT, INVALID_COLOUR, EWW_WATCH), SetPadding(2, 2, 2, 2), SetResize(1, 1), SetFill(1, 1),
		EndContainer( ),
	EndContainer( ),
	/* Status Bar with resize buton */
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_PANEL, COLOUR_GREY), SetFill(1, 1), SetResize(1, 0), EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_GREY),
	EndContainer( ),
};

/**
 * Watch Company Window Descriptor
 */
static WindowDesc _watch_company_desc(
	WDP_AUTO, NULL, 300, 257,
	WC_WATCH_COMPANY, WC_NONE,
	WDF_NO_FOCUS,
	_nested_watch_company_widgets, lengthof( _nested_watch_company_widgets )
);

//static int WatchCompany::button1_state[MAX_COMPANIES];

/** Watch Company Class Constructor
 * @param desc Window Descriptor The Window Descriptor
 * @param window_number The window number for the class
 * @param company_to_watch Company ID for watching a particular company
 */
WatchCompany::WatchCompany(WindowDesc *desc, int window_number, CompanyID company_to_watch = INVALID_COMPANY ) : Window(desc)
{
	this->watched_company = company_to_watch;

	this->InitNested(window_number);
	this->owner = this->watched_company;

	/* Reset activity and client count for all companies */
	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		this->company_activity[i] = 0;
		this->company_count_client[i] = 0;
	}
	
	GetString( this->company_name, STR_JUST_NOTHING, lastof(this->company_name) );
	
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
	switch (widget) {
		case EWW_CAPTION:
			SetDParamStr( 0, this->company_name );
			break;
	}
}

void WatchCompany::DrawWidget(const Rect &r, int widget) const
{
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
	/* Has Client Blot */
	if (IsInsideMM( widget, EWW_HAS_CLIENT_FIRST, EWW_HAS_CLIENT_LAST + 1 )) {
		if ( Company::IsValidID( widget-EWW_HAS_CLIENT_FIRST ) ) {
			/* Draw the Blot only if Company Exists */
			Dimension sprite_size = GetSpriteSize(SPR_BLOT);
#ifdef ENABLE_NETWORK
			if (!_networking) { // Local game, draw the Blot
				DrawSprite(SPR_BLOT, Company::IsValidAiID(widget - EWW_HAS_CLIENT_FIRST) ? PALETTE_TO_ORANGE : PALETTE_TO_GREEN, (r.left + r.right - sprite_size.width) / 2, (r.top + r.bottom - sprite_size.height) / 2 );
			} else { // Network game, draw the blot according to company client count
				DrawSprite(SPR_BLOT, this->company_count_client[widget-EWW_HAS_CLIENT_FIRST] > 0 ? (company_activity[widget-EWW_HAS_CLIENT_FIRST] > 0 ? PALETTE_TO_RED : PALETTE_TO_GREEN) : PALETTE_TO_GREY, (r.left + r.right - sprite_size.width) / 2, (r.top + r.bottom - sprite_size.height) / 2 );
			}
#else
			DrawSprite(SPR_BLOT, Company::IsValidAiID(widget-EWW_HAS_CLIENT_FIRST) ? PALETTE_TO_ORANGE : PALETTE_TO_GREEN, (r.left + r.right - sprite_size.width) / 2, (r.top + r.bottom - sprite_size.height) / 2 );
#endif
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
	const ViewPort *vp = IsPtInWindowViewport(this, _cursor.pos.x, _cursor.pos.y);
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

void WatchCompany::OnClick(Point pt, int widget, int click_count)
{
	if (IsInsideMM(widget, EWW_PB_COMPANY_FIRST, EWW_PB_COMPANY_LAST + 1)) {
		/* Click on Company Button */
		if (!this->IsWidgetDisabled(widget)) {
			if (this->watched_company != INVALID_COMPANY) {
				/* Raise the watched company button  */
				this->RaiseWidget(this->watched_company + EWW_PB_COMPANY_FIRST);
			}
			if (this->watched_company == (CompanyID)(widget - EWW_PB_COMPANY_FIRST)) {
				/* Stop watching watched_company */
				this->watched_company = INVALID_COMPANY;
                                GetString( this->company_name, STR_JUST_NOTHING, lastof(this->company_name) );
			} else {
				/* Lower the new watched company button */
				this->watched_company = (CompanyID)(widget - EWW_PB_COMPANY_FIRST);
				this->LowerWidget(this->watched_company + EWW_PB_COMPANY_FIRST);
				Company *c = Company::Get( this->watched_company );
				SetDParam( 0, c->index );
				GetString( this->company_name, STR_COMPANY_NAME, lastof(this->company_name) );
				
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
#ifdef ENABLE_NETWORK	
	else if ( IsInsideMM(widget, EWW_HAS_CLIENT_FIRST, EWW_HAS_CLIENT_LAST + 1)) {
		if(_networking && Company::IsValidID(widget - EWW_HAS_CLIENT_FIRST)){
			ShowNetworkChatQueryWindow(DESTTYPE_TEAM, widget - EWW_HAS_CLIENT_FIRST);
		}
	} 
#endif
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
				ShowWatchWindow( this->watched_company );
				break;
		}
	}
}

void WatchCompany::OnInvalidateData(int data, bool gui_scope)
{
	/* Disable the companies who are not active */
	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		this->SetWidgetDisabledState(EWW_PB_COMPANY_FIRST + i , !Company::IsValidID(i) );
		this->SetWidgetDisabledState(EWW_PB_ACTION1_FIRST + i , !Company::IsValidID(i) );
	}
	/* Check if the currently selected company is still active. */
	if (this->watched_company != INVALID_COMPANY) {
		/* Make sure the widget is lowered */
		this->LowerWidget(EWW_PB_COMPANY_FIRST + this->watched_company);
		/* Check if the watched Company is still a valid one */
		if (!Company::IsValidID(this->watched_company)) {
			/* Invalid Company Raise the associated widget. */
			this->RaiseWidget(this->watched_company + EWW_PB_COMPANY_FIRST );
			this->watched_company = INVALID_COMPANY;
			GetString( this->company_name, STR_JUST_NOTHING, lastof(this->company_name) );
		} else {
			Company *c = Company::Get( this->watched_company );
			SetDParam( 0, c->index );
			GetString( this->company_name, STR_COMPANY_NAME, lastof(this->company_name) );
		}
	} else {
		GetString( this->company_name, STR_JUST_NOTHING, lastof(this->company_name) );
	  
	}
#ifdef ENABLE_NETWORK
	if (_networking) { // Local game, draw the Blot
		/* Reset company count - network only */
		for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			this->company_count_client[i] = 0;
		}
		/* Calculate client count into company - network only */
		NetworkClientInfo *ci;
		FOR_ALL_CLIENT_INFOS( ci ) {
			if (Company::IsValidID(ci->client_playas)) {
				company_count_client[ci->client_playas]+=1;
			}
		}
	}
#endif
	HandleZoomMessage(this, this->viewport, EWW_ZOOMIN, EWW_ZOOMOUT);
}

void WatchCompany::ScrollToTile( TileIndex tile )
{
	/* Scroll window to the tile, only if not zero */
	if (tile != 0) {
		ScrollWindowTo( TileX(tile) * TILE_SIZE + TILE_SIZE / 2, TileY(tile) * TILE_SIZE + TILE_SIZE / 2, -1, this );
	}
}

/** OnDoCommand function - Called by the DoCommand
 *  @param company The company ID who's client is building
 *  @param tile The tile number where action took place
 */
void WatchCompany::OnDoCommand( CompanyByte company, TileIndex tile )
{
	/* Check if its my company */
	if (this->watched_company == company)
	{
		this->ScrollToTile( tile );
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
void WatchCompany::OnTick()
{
	bool set_dirty = false;
	for (CompanyID i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		if ( this->company_activity[i]>0 ) {
			this->company_activity[i]--;
			if ( this->company_activity[i]==0 ) {
				set_dirty = true;
			}
		}
	}
	/* If one company_activity reaches 0, then redraw */
	if (set_dirty) {
		this->SetDirty();
	}
}

void ShowWatchWindow( CompanyID company_to_watch = INVALID_COMPANY )
{
	int i = 0;
	/* find next free window number for watch viewport */
	while (FindWindowById(WC_WATCH_COMPANY, i) != NULL) i++;	
	new WatchCompany( &_watch_company_desc, i, company_to_watch );
}
