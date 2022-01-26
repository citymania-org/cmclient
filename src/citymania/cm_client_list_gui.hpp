#ifndef CM_CLIENT_LIST_GUI_HPP
#define CM_CLIENT_LIST_GUI_HPP

namespace citymania {

void SetClientListDirty();
void UndrawClientList(int left, int top, int right, int bottom);
void DrawClientList();

}  // namespace citymania

#endif
