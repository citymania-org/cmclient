#include "../stdafx.h"

#include "cm_town_gui.hpp"

#include "../gui.h"  // ShowExtraViewportWindow
#include "../hotkeys.h"  // Hotkey
#include "../goal_base.h"  // Goal::Iterate
#include "../strings_type.h"  // StringID
#include "../strings_func.h"  // GetString
#include "../textbuf_gui.h"  // QueryStringFlag
#include "../viewport_func.h"  // ScrollMainWindowToTile
#include "../zoom_func.h"  // ScaleGUITrad
#include "../widgets/town_widget.h"
#include "../table/strings.h"

#include "cm_commands.hpp"
#include "cm_hotkeys.hpp"
#include <optional>

void ShowTownAuthorityWindow(uint town);  // town_gui.cpp

namespace citymania {

// TODO replace with pair or remove
struct CargoX {
	int id;
	int from;
};

bool TownExecuteAction(const Town *town, TownAction action) {
	if(!(action == TownAction::BuildStatue && town->statues.Test(_current_company))) { //don't built statue when there is one
		return cmd::DoTownAction(town->xy, town->index, action)
			.with_error(STR_ERROR_CAN_T_DO_THIS)
			.post();
	}
	return false;
}

void DrawExtraTownInfo (Rect &r, const Town *town, uint line, bool show_house_states_info) {
	//real pop and rating
	DrawString(r, GetString(
		CM_STR_TOWN_VIEW_REALPOP_RATE,
		town->cm.real_population,
		town->ratings[_current_company]
	));
	r.top += line;
	//town stats
	// int grow_rate = 0;
	// if(town->growth_rate == TOWN_GROW_RATE_CUSTOM_NONE) grow_rate = 0;
	// else grow_rate = TownTicksToDays((town->growth_rate & ~TOWN_GROW_RATE_CUSTOM) + 1);

	// SetDParam(2, town->grow_counter < 16000 ? TownTicksToDays(town->grow_counter + 1) : -1);
	DrawString(r, GetString(
		CM_STR_TOWN_VIEW_GROWTH,
		town->growth_rate,
		town->flags.Test(TownFlag::CustomGrowth) ? CM_STR_TOWN_VIEW_GROWTH_RATE_CUSTOM : STR_EMPTY,
		town->grow_counter,
		town->time_until_rebuild,
		town->flags.Test(TownFlag::IsGrowing) ? 1 : 0,
		town->fund_buildings_months
	));
	r.top += line;

	if (show_house_states_info) {
		DrawString(r, GetString(
			CM_STR_TOWN_VIEW_HOUSE_STATE,
			town->cm.houses_constructing,
			town->cm.houses_reconstructed_last_month,
			town->cm.houses_demolished_last_month
		));
		r.top += line;
	}

	///houses stats
	DrawString(r, GetString(
		CM_STR_TOWN_VIEW_GROWTH_TILES,
		town->cm.hs_total,
		town->cm.hs_last_month,
		town->cm.cs_total,
		town->cm.cs_last_month,
		town->cm.hr_total,
		town->cm.hr_last_month
	));
	r.top += line;
}

bool CB_sortCargoesByFrom(CargoX first, CargoX second){
	return (first.from < second.from) ? true : false;
}

struct CBTownWindow : Window {
private:
	Town *town;
	std::list<CargoX> cargoes;

	enum CBTownWindowPlanes {
		CBTWP_MP_GOALS = 0,
		CBTWP_MP_CB,
	};

public:
	CBTownWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		for (uint i = 0; i < NUM_CARGO ; i++) {
			CargoX c;
			c.id = i;
			c.from = CB_GetFrom(i);
			this->cargoes.push_back(c);
		}
		cargoes.sort(CB_sortCargoesByFrom);
		this->town = Town::Get(window_number);
		this->InitNested(window_number);

		if(this->town->cm.fund_regularly.Test(_local_company)) this->LowerWidget(WID_CB_FUND_REGULAR);
		if(this->town->cm.do_powerfund.Test(_local_company)) this->LowerWidget(WID_CB_POWERFUND);
		if(this->town->cm.advertise_regularly.Test(_local_company)) this->LowerWidget(WID_CB_ADVERT_REGULAR);
	}

	void OnClick([[maybe_unused]] Point pt, int widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_CB_CENTER_VIEW:
				if (citymania::_fn_mod) {
					ShowExtraViewportWindow(this->town->xy);
				} else {
					ScrollMainWindowToTile(this->town->xy);
				}
				break;
			case WID_CB_ADVERT:
				TownExecuteAction(this->town, TownAction::AdvertiseLarge);
				break;
			case WID_CB_FUND:
				TownExecuteAction(this->town, TownAction::FundBuildings);
				break;
			case WID_CB_FUND_REGULAR:
				this->town->cm.fund_regularly.Flip(_local_company);
				this->SetWidgetLoweredState(widget, this->town->cm.fund_regularly.Test(_local_company));
				this->SetWidgetDirty(widget);
				break;
			case WID_CB_POWERFUND:
				this->town->cm.do_powerfund.Flip(_local_company);
				this->SetWidgetLoweredState(widget, this->town->cm.do_powerfund.Test(_local_company));
				this->SetWidgetDirty(widget);
				break;
			case WID_CB_ADVERT_REGULAR:
				if (this->town->cm.advertise_regularly.None()) {
					auto str = GetString(STR_JUST_INT, ToPercent8(this->town->cm.ad_rating_goal));
					ShowQueryString(str, CM_STR_CB_ADVERT_REGULAR_RATING_TO_KEEP,
					                4, this, CS_NUMERAL, QueryStringFlag::AcceptUnchanged);
				} else this->OnQueryTextFinished(std::nullopt);
				break;
			case WID_CB_TOWN_VIEW: // Town view window
				ShowTownViewWindow(this->window_number);
				break;
			case WID_CB_SHOW_AUTHORITY: // town authority
				ShowTownAuthorityWindow(this->window_number);
				break;
		}
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		if (!str.has_value()) this->town->cm.advertise_regularly.Set(_local_company);
		else this->town->cm.advertise_regularly.Reset(_local_company);
		this->town->cm.ad_ref_goods_entry = std::nullopt;
		this->SetWidgetLoweredState(WID_CB_ADVERT_REGULAR, this->town->cm.advertise_regularly.Test(_local_company));
		this->SetWidgetDirty(WID_CB_ADVERT_REGULAR);

		if (!str.has_value()) return;
		uint val = Clamp(str->empty()? 0 : strtol(str->c_str(), NULL, 10), 1, 100);
		this->town->cm.ad_rating_goal = ((val << 8) + 255) / 101;
	}

    std::string GetWidgetString(WidgetID widget, StringID stringid) const override
	{
		if (widget != WID_CB_CAPTION) return this->Window::GetWidgetString(widget, stringid);
		return GetString(CM_STR_TOWN_VIEW_CB_CAPTION, this->town->index);
	}

	const Company *GetCompany() const {
		for (Company *c : Company::Iterate()) {
			if (c->location_of_HQ != INVALID_TILE
			    	&& DistanceMax(c->location_of_HQ, this->town->xy) < 11)
				return c;
		}
		return nullptr;
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override {
		static const uint EXP_TOPPADDING = 5;
		static const uint EXP_LINESPACE  = 2; // Amount of vertical space for a horizontal (sub-)total line.

		switch(widget){
			case WID_CB_DETAILS:
				size.height = (GetCharacterHeight(FS_NORMAL) + EXP_LINESPACE) * 5;
				break;
			case WID_CB_GOALS: {
				uint desired_height = 0;
				auto company = GetCompany();
				if (company) {
					for(const Goal *g : Goal::Iterate()) {
						if (g->company == company->index) {
							desired_height++;
						}
					}
				}
				if (desired_height > 0)
					size.height = desired_height * (GetCharacterHeight(FS_NORMAL) + EXP_LINESPACE) + EXP_TOPPADDING - EXP_LINESPACE;
				break;
			}
			case WID_CB_CARGO_NAME:
			case WID_CB_CARGO_AMOUNT:
			case WID_CB_CARGO_REQ:
			case WID_CB_CARGO_STORE:
			case WID_CB_CARGO_STORE_PCT:
			case WID_CB_CARGO_FROM:
			case WID_CB_CARGO_PREVIOUS: {
				uint desired_height = 0;
				for(CargoType cargo = 0; cargo < NUM_CARGO; cargo++){
					if(CB_GetReq(cargo) > 0) desired_height++;
				}
				if (desired_height > 0)
					size.height = desired_height * (GetCharacterHeight(FS_NORMAL) + EXP_LINESPACE) + EXP_TOPPADDING - EXP_LINESPACE;
				break;
			}
		}
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		static const uint EXP_LINESPACE  = GetCharacterHeight(FS_NORMAL) + 2;
		Rect tr = r.Shrink(WidgetDimensions::scaled.framerect);

		switch(widget){
			case WID_CB_DETAILS:{
				// growing
				if(this->town->cm.cb.growth_state == TownGrowthState::GROWING)
					DrawString(tr, CM_STR_TOWN_CB_GROWING);
				else DrawString(tr, CM_STR_TOWN_CB_NOT_GROWING);
				tr.top += GetCharacterHeight(FS_NORMAL);

				// population
				DrawString(tr, GetString(
					STR_TOWN_VIEW_POPULATION_HOUSES,
					this->town->cache.population,
					this->town->cache.num_houses
				));
				tr.top += GetCharacterHeight(FS_NORMAL);

				DrawExtraTownInfo(tr, this->town, EXP_LINESPACE, false);

				// regular funding
				if (this->town->cm.fund_regularly.Any()) {
					DrawString(tr, CM_STR_CB_FUNDED_REGULARLY);
					tr.top += EXP_LINESPACE;
				}
				break;
			}
			case WID_CB_GOALS: {
				auto company = GetCompany();
				if (!company) break;
				for(const Goal *g : Goal::Iterate()) {
					if (g->company != company->index)
						continue;
					DrawString(tr, GetString(STR_GOALS_TEXT, g->text.GetDecodedString()));
					tr.top += EXP_LINESPACE;
				}
				break;
			}
			/* Citybuilder things*/
			case WID_CB_CARGO_NAME:
			case WID_CB_CARGO_AMOUNT:
			case WID_CB_CARGO_REQ:
			case WID_CB_CARGO_STORE:
			case WID_CB_CARGO_STORE_PCT:
			case WID_CB_CARGO_FROM:
			case WID_CB_CARGO_PREVIOUS: {
				if (this->town->larger_town) break;
				uint delivered;
				uint requirements;
				uint from;
				StringID string_to_draw;

				//for cycle
				std::list<CargoX> cargoes2 = this->cargoes;
				std::list<CargoX>::iterator it2;
				for (it2 = cargoes2.begin(); it2 != cargoes2.end(); ++it2) {
					CargoX cargox;
					cargox = *it2;
					if (it2 == cargoes2.begin()) { // header
						// FIXME rtl support
						DrawString(tr.Shrink(ScaleGUITrad(14), 0, 0, 0),
							(CM_STR_TOWN_GROWTH_HEADER_CARGO + widget - WID_CB_CARGO_NAME), TC_FROMSTRING,
							(widget == WID_CB_CARGO_NAME) ? SA_LEFT : SA_RIGHT);

						tr.top += EXP_LINESPACE;
					}

					const CargoSpec *cargos = CargoSpec::Get(cargox.id);
					//cargo needed?
					if (!cargos->IsValid() || CB_GetReq(cargos->Index()) == 0) continue;

					from = CB_GetFrom(cargos->Index());
					switch(widget) {
						case WID_CB_CARGO_NAME: {
							// FIXME scaling
							GfxFillRect(tr.left, tr.top + 1, tr.left + 8, tr.top + 6, PC_BLACK);
							GfxFillRect(tr.left + 1, tr.top + 2, tr.left + 7, tr.top + 5, cargos->legend_colour);

							DrawString(tr.Shrink(ScaleGUITrad(14), 0, 0, 0), GetString(CM_STR_TOWN_CB_CARGO_NAME, cargos->name));
							break;
						}
						case WID_CB_CARGO_AMOUNT: {
							delivered = this->town->cm.cb.delivered[cargos->Index()];
							requirements = CB_GetTownReq(this->town->cache.population, CB_GetReq(cargos->Index()), from, true);

							//when required
							if (this->town->cache.population >= from) {
								if((delivered + (uint)this->town->cm.cb.stored[cargos->Index()]) >= requirements) string_to_draw = CM_STR_TOWN_CB_CARGO_AMOUNT_GOOD;
								else string_to_draw = CM_STR_TOWN_CB_CARGO_AMOUNT_BAD;
							}
							//when not required -> all faded
							else string_to_draw = CM_STR_TOWN_CB_CARGO_AMOUNT_NOT;

							DrawString(tr, GetString(string_to_draw, delivered), TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_REQ: {
							requirements = CB_GetTownReq(this->town->cache.population, CB_GetReq(cargos->Index()), from, true);
							 //when required
							string_to_draw = (this->town->cache.population >= from) ? CM_STR_TOWN_CB_CARGO_REQ_YES : CM_STR_TOWN_CB_CARGO_REQ_NOT;
							DrawString(tr, GetString(string_to_draw, requirements), TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_PREVIOUS: {
							requirements = CB_GetTownReq(this->town->cache.population, CB_GetReq(cargos->Index()), from, true);
							if (this->town->cache.population >= from){
								// if (this->town->delivered_enough[cargos->Index()]) {
									string_to_draw = (this->town->cm.cb.delivered_last_month[cargos->Index()] >= requirements) ? CM_STR_TOWN_CB_CARGO_PREVIOUS_YES : CM_STR_TOWN_CB_CARGO_PREVIOUS_EDGE;
								// }
								// else string_to_draw = STR_TOWN_CB_CARGO_PREVIOUS_BAD;
							}
							else string_to_draw = CM_STR_TOWN_CB_CARGO_PREVIOUS_NOT;

							DrawString(tr, GetString(string_to_draw, this->town->cm.cb.delivered_last_month[cargos->Index()]), TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_STORE: {
							if (CB_GetDecay(cargos->Index()) == 100) string_to_draw = CM_STR_TOWN_CB_CARGO_STORE_DECAY;  //when 100% decay
							else {
								if (this->town->cache.population >= from) string_to_draw = CM_STR_TOWN_CB_CARGO_STORE_YES;  //when required
								else string_to_draw = CM_STR_TOWN_CB_CARGO_STORE_NOT;
							}

							DrawString(tr, GetString(string_to_draw, this->town->cm.cb.stored[cargos->Index()]), TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_STORE_PCT: {
							uint max_storage = CB_GetMaxTownStorage(this->town, cargos->Index());
							std::string str;
							if (CB_GetDecay(cargos->Index()) == 100 || !max_storage) str = GetString(CM_STR_TOWN_CB_CARGO_STORE_DECAY);  //when 100% decay
							else {
								str = GetString(
									this->town->cache.population >= from ? CM_STR_TOWN_CB_CARGO_STORE_PCT_YES : CM_STR_TOWN_CB_CARGO_STORE_PCT_NOT,
									100 * this->town->cm.cb.stored[cargos->Index()] / max_storage
								);
							}

							DrawString(tr, str, TC_FROMSTRING, SA_RIGHT);
							break;
						}
						case WID_CB_CARGO_FROM: {
							string_to_draw = (this->town->cache.population >= from) ? CM_STR_TOWN_CB_CARGO_FROM_YES : CM_STR_TOWN_CB_CARGO_FROM_NOT; //when required

							DrawString(tr, GetString(string_to_draw, from), TC_FROMSTRING, SA_RIGHT);
							break;
						}
						//last case
					}
					//switch
					tr.top += EXP_LINESPACE;
					//cargo needed?
				}
				//for cycle
			}
			break;
		}
		/* Citybuilder things enabled*/
	}

	void OnPaint() override {
		if (!this->IsShaded()) {
			int plane = CB_Enabled() ? CBTWP_MP_CB : CBTWP_MP_GOALS;
			NWidgetStacked *wi = this->GetWidget<NWidgetStacked>(WID_CB_SELECT_REQUIREMENTS);
			if (plane != wi->shown_plane) {
				wi->SetDisplayedPlane(plane);
				this->InvalidateData();
				return;
			}
		}
		this->DrawWidgets();
	}


	EventState OnHotkey(int hotkey) override
	{
		if (hotkey == CM_THK_BUILD_STATUE) {
			TownExecuteAction(this->town, TownAction::BuildStatue);
			return ES_HANDLED;
		}
		return ES_NOT_HANDLED;
	}

	static inline HotkeyList hotkeys{"cm_town_cb_view", {
		Hotkey((uint16)0, "location", WID_TV_CENTER_VIEW),
		Hotkey((uint16)0, "local_authority", WID_TV_SHOW_AUTHORITY),
		Hotkey((uint16)0, "cb_window", CM_WID_TV_CB),
		Hotkey(WKC_CTRL | 'S', "build_statue", CM_THK_BUILD_STATUE),
	}};
};

static const NWidgetPart _nested_cb_town_widgets[] = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN, WID_CB_CAPTION), SetStringTip(CM_STR_TOWN_VIEW_CB_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_PUSHIMGBTN, COLOUR_BROWN, WID_CB_CENTER_VIEW), SetAspect(WidgetDimensions::ASPECT_LOCATION), SetSpriteTip(SPR_GOTO_LOCATION, STR_TOWN_VIEW_CENTER_TOOLTIP),
		NWidget(WWT_SHADEBOX, COLOUR_BROWN),
		NWidget(WWT_DEFSIZEBOX, COLOUR_BROWN),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN),
		NWidget(NWID_VERTICAL),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),  SetResize(1, 0), SetFill(1, 0),
			NWidget(NWID_HORIZONTAL),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CB_DETAILS), SetMinimalSize(66, 0), SetResize(1, 0), SetFill(1, 0),
				NWidget(NWID_VERTICAL),
					NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
						NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_ADVERT),SetMinimalSize(33, 12),SetFill(1, 0), SetStringTip(CM_STR_CB_LARGE_ADVERTISING_CAMPAIGN),
						NWidget(NWID_SPACER), SetMinimalSize(2, 0),
						NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_FUND),SetMinimalSize(33, 12),SetFill(1, 0), SetStringTip(CM_STR_CB_NEW_BUILDINGS),
						NWidget(NWID_SPACER), SetMinimalSize(4, 0),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(0, 2),
					NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
						NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_CB_ADVERT_REGULAR),SetMinimalSize(33, 12),SetFill(1, 0), SetStringTip(CM_STR_CB_ADVERT_REGULAR, CM_STR_CB_ADVERT_REGULAR_TT),
 						NWidget(NWID_SPACER), SetMinimalSize(2, 0),
						NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_CB_FUND_REGULAR),SetMinimalSize(33, 12),SetFill(1, 0), SetStringTip(CM_STR_CB_FUND_REGULAR, CM_STR_CB_FUND_REGULAR_TT),
						NWidget(NWID_SPACER), SetMinimalSize(4, 0),
					EndContainer(),
					NWidget(NWID_SPACER), SetMinimalSize(0, 2),
					NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize),
  						NWidget(NWID_SPACER), SetMinimalSize(33, 0), SetFill(1, 0),
  						NWidget(NWID_SPACER), SetMinimalSize(2, 0),
						NWidget(WWT_TEXTBTN, COLOUR_BROWN, WID_CB_POWERFUND),SetMinimalSize(33, 12),SetFill(1, 0), SetStringTip(CM_STR_CB_POWERFUND, CM_STR_CB_POWERFUND_TT),
						NWidget(NWID_SPACER), SetMinimalSize(4, 0),
					EndContainer(),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 5),  SetResize(1, 0), SetFill(1, 0),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WID_CB_SELECT_REQUIREMENTS),
				NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CB_GOALS),SetMinimalSize(50 + 35 + 35 + 40 + 35 + 30, 0), SetResize(1, 0), SetFill(1, 0),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CB_CARGO_NAME),SetMinimalSize(50, 0), SetResize(0, 0), SetFill(1, 0),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CB_CARGO_AMOUNT),SetMinimalSize(35, 0), SetResize(1, 0), SetFill(0, 0),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CB_CARGO_REQ),SetMinimalSize(35, 0), SetResize(1, 0), SetFill(0, 0),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CB_CARGO_PREVIOUS),SetMinimalSize(40, 0),  SetResize(1, 0), SetFill(0, 0),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CB_CARGO_STORE),SetMinimalSize(35, 0), SetResize(1, 0), SetFill(0, 0),
					NWidget(WWT_EMPTY, INVALID_COLOUR, WID_CB_CARGO_STORE_PCT),SetMinimalSize(30, 0), SetResize(1, 0), SetFill(0, 0),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_SPACER), SetMinimalSize(0, 0), SetResize(1, 0), SetFill(1, 1),
		EndContainer(),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_TOWN_VIEW), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(CM_STR_CB_GUI_TOWN_VIEW_BUTTON, CM_STR_CB_GUI_TOWN_VIEW_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_BROWN, WID_CB_SHOW_AUTHORITY), SetMinimalSize(40, 12), SetFill(1, 1), SetResize(1, 0), SetStringTip(STR_TOWN_VIEW_LOCAL_AUTHORITY_BUTTON, STR_TOWN_VIEW_LOCAL_AUTHORITY_TOOLTIP),
		EndContainer(),
		NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
	EndContainer(),
};

static WindowDesc _cb_town_desc(
	WDP_AUTO, "cm_town_cb", 160, 30,
	CM_WC_CB_TOWN, WC_NONE,
	{},
	_nested_cb_town_widgets,
	&CBTownWindow::hotkeys
);

void ShowCBTownWindow(TownID town) {
	AllocateWindowDescFront<CBTownWindow>(_cb_town_desc, town);
}

} // namespace citymania
