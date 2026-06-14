#include "../stdafx.h"

#include "cm_commands.hpp"

#include "../dropdown_func.h"  // MakeDropDownListStringItem
#include "../ini_type.h"  // IniFile, IniGroup
#include "../strings_func.h"  // GetString

namespace citymania {

static const std::string CFG_IDENTITY_FILE  = "cmidentities.cfg";
std::vector<std::tuple<std::string, std::string, std::string>> _identities;
int _selected_identity = 0;

void LoadIdentitiesConfig() {
	IniFile ini;
	ini.LoadFromDisk(CFG_IDENTITY_FILE, BASE_DIR);
	_identities.clear();
	_selected_identity = 0;
	for (const IniGroup &group : ini.groups) {
		Debug(net, 0, "IDENTITY Group found: {}", group.name);

		auto name_item = group.GetItem("name");
		if (name_item == nullptr) {
			Debug(net, 0, "Invalid identity '{}' in {}: missing 'name'", group.name, CFG_IDENTITY_FILE);
			continue;
		}
		auto name = name_item->value.value_or("");
		if (name.empty()) {
			Debug(net, 0, "Invalid identity '{}' in {}: 'name' is empty", group.name, CFG_IDENTITY_FILE);
			continue;
		}

		auto pkey_item = group.GetItem("client_public_key");
		if (pkey_item == nullptr) {
			Debug(net, 0, "Invalid identity '{}' in {}: missing 'client_public_key'", group.name, CFG_IDENTITY_FILE);
			continue;
		}
		auto pkey = pkey_item->value.value_or("");
		if (pkey.empty()) {
			Debug(net, 0, "Invalid identity '{}' in {}: 'client_public_key' is empty", group.name, CFG_IDENTITY_FILE);
			continue;
		}

		auto skey_item = group.GetItem("client_secret_key");
		if (skey_item == nullptr) {
			Debug(net, 0, "Invalid identity '{}' in {}: missing 'client_secret_key'", group.name, CFG_IDENTITY_FILE);
			continue;
		}
		auto skey = skey_item->value.value_or("");
		if (skey.empty()) {
			Debug(net, 0, "Invalid identity '{}' in {}: 'client_secret_key' is empty", group.name, CFG_IDENTITY_FILE);
			continue;
		}

		_identities.emplace_back(name, pkey, skey);
	}
}

bool HasMultipleIdentities() {
	return !_identities.empty();
}

DropDownList BuildIdentityDropDownList() {
	DropDownList list;

	list.push_back(MakeDropDownListStringItem(CM_STR_NETWORK_SERVER_LIST_DEFAULT_IDENTITY, 0));
	for (auto &id : _identities) {
		list.push_back(MakeDropDownListStringItem(GetString(STR_JUST_RAW_STRING, std::get<0>(id)), list.size()));
	}

	return list;
}

void SetSelectedIdentity(int index) {
	_selected_identity = index;
}

std::string GetSelectedIdentity() {
	if (_selected_identity == 0 || _selected_identity > _identities.size())
		return GetString(CM_STR_NETWORK_SERVER_LIST_DEFAULT_IDENTITY);
	auto &id = _identities[_selected_identity - 1];
	return std::get<0>(id);
}

std::string &GetPublicKey() {
	if (_selected_identity == 0 || _selected_identity > _identities.size())
		return _settings_client.network.client_public_key;
	auto &id = _identities[_selected_identity - 1];
	return std::get<1>(id);
}

std::string &GetSecretKey() {
	if (_selected_identity == 0 || _selected_identity > _identities.size())
		return _settings_client.network.client_secret_key;
	auto &id = _identities[_selected_identity - 1];
	return std::get<2>(id);
}


}  // namespace citymania
