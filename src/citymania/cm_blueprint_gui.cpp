#include "../stdafx.h"

#include "cm_blueprint.hpp"
#include "cm_blueprint_gui.hpp"

#include "../window_gui.h"
#include "../window_func.h"
#include "../strings_func.h"
#include "../textbuf_gui.h"
#include "../error.h"
#include "../dropdown_type.h"
#include "../dropdown_func.h"
#include "../core/geometry_func.hpp"

#include "../widgets/cm_blueprint_widget.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#endif

namespace citymania {

static constexpr uint BPM_ROWS_VISIBLE = 8; ///< Number of slot rows shown before scrolling kicks in.

#ifdef _WIN32
/** Copy an OTTD (UTF-8) string into a fixed wchar_t buffer used as an OPENFILENAMEW in/out parameter. */
static void FillFileBuffer(wchar_t *buf, size_t buf_len, const std::string &value)
{
	std::wstring wide = OTTD2FS(value);
	size_t n = std::min(wide.size(), buf_len - 1);
	wide.copy(buf, n);
	buf[n] = L'\0';
}

/** Native "Save As" dialog so the filename can be browsed to and edited before exporting. Returns the chosen full path, or empty if cancelled. */
static std::string Win32SaveBlueprintDialog(const std::string &suggested_name)
{
	wchar_t file_buf[MAX_PATH];
	FillFileBuffer(file_buf, std::size(file_buf), suggested_name);
	std::wstring initial_dir = OTTD2FS(GetBlueprintsDir());

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = L"CityMania Blueprint (*.cmbp)\0*.cmbp\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = file_buf;
	ofn.nMaxFile = static_cast<DWORD>(std::size(file_buf));
	ofn.lpstrInitialDir = initial_dir.c_str();
	ofn.lpstrDefExt = L"cmbp";
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetSaveFileNameW(&ofn)) return {};
	return FS2OTTD(std::wstring(file_buf));
}

/** Native "Open" dialog so any .cmbp file on disk can be picked, not just ones in the blueprints folder. Returns the chosen full path, or empty if cancelled. */
static std::string Win32OpenBlueprintDialog()
{
	wchar_t file_buf[MAX_PATH] = {};
	std::wstring initial_dir = OTTD2FS(GetBlueprintsDir());

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = L"CityMania Blueprint (*.cmbp)\0*.cmbp\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = file_buf;
	ofn.nMaxFile = static_cast<DWORD>(std::size(file_buf));
	ofn.lpstrInitialDir = initial_dir.c_str();
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetOpenFileNameW(&ofn)) return {};
	return FS2OTTD(std::wstring(file_buf));
}
#endif

/** Blueprint manager: named slots, save/load into them, and file export/import. */
struct BlueprintManagerWindow : Window {
	/** Which text query is currently open, so OnQueryTextFinished knows how to handle the result. */
	enum class PendingQuery : uint8_t { NONE, RENAME, EXPORT_FILENAME };

	Scrollbar *vscroll = nullptr;
	int selected = -1; ///< Selected slot, -1 if none.
	std::vector<std::string> import_files; ///< Filenames backing the currently-open import dropdown, if any.
	PendingQuery pending_query = PendingQuery::NONE;

	BlueprintManagerWindow(WindowDesc &desc, WindowNumber window_number) : Window(desc)
	{
		this->CreateNestedTree();
		this->vscroll = this->GetScrollbar(WID_BPM_SCROLLBAR);
		this->vscroll->SetCount((int)MAX_BLUEPRINT_SLOTS);
		this->FinishInitNested(window_number);
	}

	std::string SlotLabel(uint slot) const
	{
		std::string name = GetBlueprintSlotName(slot);
		if (!name.empty()) return GetString(CM_STR_BLUEPRINT_MANAGER_SLOT_LINE, slot, name);
		if (IsBlueprintSlotEmpty(slot)) return GetString(CM_STR_BLUEPRINT_MANAGER_SLOT_LINE, slot, std::string("<empty>"));
		return GetString(CM_STR_BLUEPRINT_MANAGER_SLOT_LINE, slot, std::string("<unnamed>"));
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding, [[maybe_unused]] Dimension &fill, [[maybe_unused]] Dimension &resize) override
	{
		if (widget != WID_BPM_LIST) return;
		resize.height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.matrix.Vertical();
		size.height = resize.height * BPM_ROWS_VISIBLE;
	}

	void OnResize() override
	{
		this->vscroll->SetCapacityFromWidget(this, WID_BPM_LIST);
	}

	void DrawWidget(const Rect &r, int widget) const override
	{
		if (widget != WID_BPM_LIST) return;

		Rect ir = r.Shrink(WidgetDimensions::scaled.matrix);
		int row_height = GetCharacterHeight(FS_NORMAL) + WidgetDimensions::scaled.matrix.Vertical();
		int y = ir.top;

		int32_t first = this->vscroll->GetPosition();
		int32_t last = std::min<int32_t>(first + this->vscroll->GetCapacity(), (int32_t)MAX_BLUEPRINT_SLOTS);

		for (int32_t i = first; i < last; i++) {
			bool empty = IsBlueprintSlotEmpty((uint)i);
			TextColour tc = (i == this->selected) ? TC_WHITE : (empty ? TC_GREY : TC_BLACK);
			DrawString(ir.left, ir.right, y + WidgetDimensions::scaled.matrix.top, this->SlotLabel((uint)i), tc);
			y += row_height;
		}
	}

	void OnClick([[maybe_unused]] Point pt, int widget, [[maybe_unused]] int click_count) override
	{
		switch (widget) {
			case WID_BPM_LIST: {
				int32_t row = this->vscroll->GetScrolledRowFromWidget(pt.y, this, WID_BPM_LIST);
				if (row != INT32_MAX && row >= 0 && (size_t)row < MAX_BLUEPRINT_SLOTS) {
					this->selected = row;
					this->SetDirty();
				}
				break;
			}

			case WID_BPM_RENAME:
				if (this->selected < 0) break;
				this->pending_query = PendingQuery::RENAME;
				ShowQueryString(GetString(STR_JUST_RAW_STRING, GetBlueprintSlotName((uint)this->selected)),
						CM_STR_BLUEPRINT_MANAGER_RENAME_QUERY, 64, this, CS_ALPHANUMERAL, {});
				break;

			case WID_BPM_SAVE:
				if (this->selected < 0) break;
				if (!HasActiveBlueprint()) break; // nothing copied yet - leave the slot alone
				SaveBlueprint((uint)this->selected);
				this->SetDirty();
				break;

			case WID_BPM_LOAD:
				if (this->selected < 0) break;
				LoadBlueprint((uint)this->selected);
				break;

			case WID_BPM_CLEAR:
				if (this->selected < 0) break;
				ClearBlueprintSlot((uint)this->selected);
				this->SetDirty();
				break;

			case WID_BPM_EXPORT: {
				if (this->selected < 0) break;
				if (IsBlueprintSlotEmpty((uint)this->selected)) break;

				std::string name = GetBlueprintSlotName((uint)this->selected);
				if (name.empty()) name = fmt::format("slot_{}", this->selected);
				std::string suggested;
				for (char c : name) {
					suggested += (isalnum((unsigned char)c) || c == '_' || c == '-') ? c : '_';
				}
				suggested += ".cmbp";

#ifdef _WIN32
				std::string path = Win32SaveBlueprintDialog(suggested);
				if (path.empty()) break; // cancelled
				this->DoExport(path);
#else
				this->pending_query = PendingQuery::EXPORT_FILENAME;
				ShowQueryString(GetString(STR_JUST_RAW_STRING, suggested),
						CM_STR_BLUEPRINT_MANAGER_EXPORT_QUERY, 64, this, CS_ALPHANUMERAL, {});
#endif
				break;
			}

			case WID_BPM_IMPORT: {
				if (this->selected < 0) {
					ShowErrorMessage(GetEncodedString(CM_STR_BLUEPRINT_MANAGER_SELECT_SLOT_FIRST), {}, WL_ERROR);
					break;
				}

#ifdef _WIN32
				std::string path = Win32OpenBlueprintDialog();
				if (path.empty()) break; // cancelled
				this->DoImport(path);
#else
				this->import_files = ListBlueprintFiles();
				if (this->import_files.empty()) {
					ShowErrorMessage(GetEncodedString(CM_STR_BLUEPRINT_MANAGER_IMPORT_NONE), {}, WL_ERROR);
					break;
				}
				DropDownList list;
				for (size_t i = 0; i < this->import_files.size(); i++) {
					list.push_back(MakeDropDownListStringItem(std::string(this->import_files[i]), (int)i));
				}
				ShowDropDownList(this, std::move(list), -1, WID_BPM_IMPORT);
#endif
				break;
			}
		}
	}

	/** Export the selected slot to a full path and report the result to the player. */
	void DoExport(const std::string &path)
	{
		if (ExportBlueprintSlot((uint)this->selected, path)) {
			ShowErrorMessage(GetEncodedString(CM_STR_BLUEPRINT_MANAGER_EXPORT_DONE, path), {}, WL_INFO);
		} else {
			ShowErrorMessage(GetEncodedString(CM_STR_BLUEPRINT_MANAGER_EXPORT_FAILED), {}, WL_ERROR);
		}
	}

	/** Import a full path into the selected slot and report the result to the player. */
	void DoImport(const std::string &path)
	{
		std::string error;
		if (!ImportBlueprintSlot((uint)this->selected, path, error)) {
			ShowErrorMessage(GetEncodedString(CM_STR_BLUEPRINT_MANAGER_IMPORT_FAILED, error), {}, WL_ERROR);
		}
		this->SetDirty();
	}

	void OnDropdownSelect(WidgetID widget, int index, [[maybe_unused]] int click_result) override
	{
		if (widget != WID_BPM_IMPORT) return;
		if (this->selected < 0) return;
		if (index < 0 || (size_t)index >= this->import_files.size()) return;

		this->DoImport(GetBlueprintsDir() + this->import_files[index]);
	}

	void OnQueryTextFinished(std::optional<std::string> str) override
	{
		PendingQuery query = this->pending_query;
		this->pending_query = PendingQuery::NONE;
		if (!str.has_value()) return;
		if (this->selected < 0) return;

		switch (query) {
			case PendingQuery::RENAME:
				RenameBlueprintSlot((uint)this->selected, *str);
				this->SetDirty();
				break;

			case PendingQuery::EXPORT_FILENAME: {
				if (str->empty()) break;
				std::string filename = *str;
				if (filename.size() < 5 || filename.compare(filename.size() - 5, 5, ".cmbp") != 0) filename += ".cmbp";
				this->DoExport(GetBlueprintsDir() + filename);
				break;
			}

			default:
				break;
		}
	}
};

static constexpr std::initializer_list<NWidgetPart> _nested_blueprint_manager_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WID_BPM_CAPTION), SetStringTip(CM_STR_BLUEPRINT_MANAGER_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_MATRIX, COLOUR_GREY, WID_BPM_LIST), SetMinimalSize(260, 0), SetFill(1, 1), SetResize(1, 1),
				SetMatrixDataTip(1, 0), SetScrollbar(WID_BPM_SCROLLBAR),
		NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WID_BPM_SCROLLBAR),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY),
		SetPIP(WidgetDimensions::unscaled.framerect.top, 2, WidgetDimensions::unscaled.framerect.bottom),
		NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(2, 2, 2),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BPM_RENAME), SetFill(1, 0), SetStringTip(CM_STR_BLUEPRINT_MANAGER_RENAME, CM_STR_BLUEPRINT_MANAGER_RENAME_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BPM_SAVE), SetFill(1, 0), SetStringTip(CM_STR_BLUEPRINT_MANAGER_SAVE, CM_STR_BLUEPRINT_MANAGER_SAVE_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BPM_LOAD), SetFill(1, 0), SetStringTip(CM_STR_BLUEPRINT_MANAGER_LOAD, CM_STR_BLUEPRINT_MANAGER_LOAD_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BPM_CLEAR), SetFill(1, 0), SetStringTip(CM_STR_BLUEPRINT_MANAGER_CLEAR, CM_STR_BLUEPRINT_MANAGER_CLEAR_TOOLTIP),
		EndContainer(),
		NWidget(NWID_HORIZONTAL, NWidContainerFlag::EqualSize), SetPIP(2, 2, 2),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BPM_EXPORT), SetFill(1, 0), SetStringTip(CM_STR_BLUEPRINT_MANAGER_EXPORT, CM_STR_BLUEPRINT_MANAGER_EXPORT_TOOLTIP),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WID_BPM_IMPORT), SetFill(1, 0), SetStringTip(CM_STR_BLUEPRINT_MANAGER_IMPORT, CM_STR_BLUEPRINT_MANAGER_IMPORT_TOOLTIP),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_RESIZEBOX, COLOUR_GREY),
};

static WindowDesc _blueprint_manager_desc(
	WDP_AUTO, "cm_blueprint_manager", 0, 0,
	WC_BLUEPRINT_MANAGER, WC_NONE,
	WindowDefaultFlag::Construction,
	_nested_blueprint_manager_widgets
);

void ShowBlueprintManagerWindow()
{
	AllocateWindowDescFront<BlueprintManagerWindow>(_blueprint_manager_desc, 0);
}

} // namespace citymania
