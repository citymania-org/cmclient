#ifndef CM_IDENTITY_GUI_HPP
#define CM_IDENTITY_GUI_HPP

#include "../dropdown_type.h"  // DropDownList
#include "../strings_type.h"  // StringID

namespace citymania {

void LoadIdentitiesConfig();
bool HasMultipleIdentities();
DropDownList BuildIdentityDropDownList();
void SetSelectedIdentity(int index);
std::string GetSelectedIdentity();
std::string &GetPublicKey();
std::string &GetSecretKey();

}  // namespace citymania

#endif
