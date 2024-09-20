#ifndef CM_COMMANDS_GUI_HPP
#define CM_COMMANDS_GUI_HPP

namespace citymania {

void ShowCommandsToolbar();
void ShowLoginWindow();
void CheckAdmin();
void ShowAdminCompanyButtons(int left, int top, int width, int company2, bool draw, bool redraw);
void JoinLastServer(int left, int top, int height);
void CreateCommunityServerList();
void ShowServerButtons(int left, int top, int height);

bool GetAdmin();

} // namespace citymania

#endif /*ZONING_H_*/
