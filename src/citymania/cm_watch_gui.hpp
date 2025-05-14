/** @file watch_gui.h GUI Functions related to watching. */

#ifndef CM_WATCH_GUI_HPP
#define CM_WATCH_GUI_HPP

#include "../window_gui.h"
#include "../company_base.h"
#include "../network/core/config.h"

namespace citymania {

#define MAX_ACTIVITY 30

enum WatchCompanyWidgets {
	EWW_CAPTION,
	EWW_LOCATION,
	EWW_PB_COMPANY_FIRST,
	EWW_PB_COMPANY_LAST  = EWW_PB_COMPANY_FIRST + MAX_COMPANIES - 1,
	EWW_HAS_CLIENT_FIRST,
	EWW_HAS_CLIENT_LAST = EWW_HAS_CLIENT_FIRST + MAX_COMPANIES - 1,
	EWW_ACTIVITY_FIRST,
	EWW_ACTIVITY_LAST = EWW_ACTIVITY_FIRST + MAX_COMPANIES - 1,
	EWW_PB_ACTION1_FIRST,
	EWW_PB_ACTION1_LAST = EWW_PB_ACTION1_FIRST + MAX_COMPANIES - 1,
	EWW_WATCH,
	EWW_ZOOMIN,
	EWW_ZOOMOUT,
	EWW_CENTER,
	EWW_NEW_WINDOW,

	EWW_KICK,
	EWW_BAN,
	EWW_BAN1,
	EWW_LOCK,
	EWW_UNLOCK,
	EWW_JOIN,
	EWW_KICKC,
	EWW_RESET,
	EWW_COMPANYW,
	EWW_COMPANYHQ,
	EWW_PRIVATEP_MESSAGE,
	EWW_PRIVATEC_MESSAGE,
	EWW_CLIENTS,
	EWW_ENABLE_SELECT,
	EWW_COMPANY_BEGIN,
	EWW_COMPANY_END = EWW_COMPANY_BEGIN + MAX_COMPANIES,
	EWW_COMPANY_FILLER = EWW_COMPANY_END,
};

enum WatchCompanyQuery {
	EWQ_BAN = 0,
	EWQ_BAN1,
};

enum WatchCompanyType {
	EWT_COMPANY = 0,
	EWT_CLIENT,
};

class WatchCompany : public Window
{
protected:
	CompanyID watched_company;                            // Company ID beeing watched.
	int company_activity[MAX_COMPANIES];                  // int array for activity blot.
	int company_count_client[MAX_COMPANIES];              // company client count.
	std::string company_name;     // company name for title display
	std::string client_name;

	int watched_client;
	WatchCompanyQuery query_widget;
	int Wtype;

	void SetWatchWindowTitle( );
	void ScrollToTile( TileIndex tile );

public:
	WatchCompany(WindowDesc &desc, int window_number, CompanyID company_to_watch, int Wtype);

	void DrawWidget(const Rect &r, int widget) const override;
	void OnClick(Point pt, int widget, int click_count) override;
	void OnResize() override;
	void OnScroll(Point delta) override;
	void OnMouseWheel(int wheel) override;
	void OnInvalidateData(int data, bool gui_scope) override;
	void SetStringParameters(int widget) const override;
	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override;
	void OnPaint() override;
	void OnQueryTextFinished(std::optional<std::string> str) override;

	void OnDoCommand(CompanyID company, TileIndex tile);
};

void ShowWatchWindow(CompanyID company_to_watch, int type);
void UpdateWatching(CompanyID company, TileIndex tile);

} // namespace citymania

#endif // COMPANY_GUI_H
