/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_client.cpp Client part of the network protocol. */

#include "../stdafx.h"
#include "network_gui.h"
#include "../saveload/saveload.h"
#include "../saveload/saveload_filter.h"
#include "../command_func.h"
#include "../console_func.h"
#include "../strings_func.h"
#include "../window_func.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../company_gui.h"
#include "../company_cmd.h"
#include "../core/random_func.hpp"
#include "../timer/timer_game_tick.h"
#include "../timer/timer_game_calendar.h"
#include "../gfx_func.h"
#include "../error.h"
#include "../rev.h"
#include "network.h"
#include "network_base.h"
#include "network_client.h"
#include "network_gamelist.h"
#include "../core/backup_type.hpp"
#include "../thread.h"
#include "../social_integration.h"

#include "table/strings.h"

#include "../town.h"
#include "network_func.h"
#include "../citymania/cm_client_list_gui.hpp"
#include "../citymania/cm_commands.hpp"
#include "../citymania/cm_newgrf_revisions.hpp"

#include "../safeguards.h"

/* This file handles all the client-commands */

void SyncCMUser(const std::string &msg);

/** Read some packets, and when do use that data as initial load filter. */
struct PacketReader : LoadFilter {
	using Buffer = std::deque<uint8_t>; ///< The underlying buffer type that's being use.

	Buffer buffer; ///< Buffer with the raw save game data.
	Buffer::const_iterator iterator; ///< Buffer we're going to write to/read from.
	size_t read_bytes = 0; ///< The total number of read bytes.

	/** Initialise everything. */
	PacketReader() : LoadFilter(nullptr)
	{
	}

	/**
	 * Simple wrapper around fwrite to be able to pass it to Packet's TransferOut.
	 * @param destination The reader to add the data to.
	 * @param source      The buffer to read data from.
	 * @param amount      The number of bytes to copy.
	 * @return The number of bytes that were copied.
	 */
	static inline ssize_t TransferOutMemCopy(PacketReader *destination, const char *source, size_t amount)
	{
		std::copy_n(source, amount, std::back_inserter(destination->buffer));
		return amount;
	}

	/**
	 * Add a packet to this buffer.
	 * @param p The packet to add.
	 */
	void AddPacket(Packet &p)
	{
		assert(this->read_bytes == 0);
		p.TransferOut(TransferOutMemCopy, this);
	}

	size_t Read(uint8_t *rbuf, size_t size) override
	{
		/* Limit the amount to read to whatever we still have. */
		size_t read_size = std::min(this->buffer.size() - this->read_bytes, size);

		std::copy_n(this->iterator, read_size, rbuf);
		std::advance(this->iterator, read_size);
		this->read_bytes += read_size;

		return read_size;
	}

	void Reset() override
	{
		this->read_bytes = 0;
		this->iterator = this->buffer.cbegin();
	}
};


/**
 * Create an emergency savegame when the network connection is lost.
 */
void ClientNetworkEmergencySave()
{
	static FiosNumberedSaveName _netsave_ctr("netsave");
	DoAutoOrNetsave(_netsave_ctr);
}


/**
 * Create a new socket for the client side of the game connection.
 * @param s The socket to connect with.
 */
ClientNetworkGameSocketHandler::ClientNetworkGameSocketHandler(SOCKET s, const std::string &connection_string) : NetworkGameSocketHandler(s), connection_string(connection_string)
{
	assert(ClientNetworkGameSocketHandler::my_client == nullptr);
	ClientNetworkGameSocketHandler::my_client = this;
}

/** Clear whatever we assigned. */
ClientNetworkGameSocketHandler::~ClientNetworkGameSocketHandler()
{
	assert(ClientNetworkGameSocketHandler::my_client == this);
	ClientNetworkGameSocketHandler::my_client = nullptr;

	delete this->GetInfo();
}

NetworkRecvStatus ClientNetworkGameSocketHandler::CloseConnection(NetworkRecvStatus status)
{
	assert(status != NETWORK_RECV_STATUS_OKAY);
	if (this->IsPendingDeletion()) return status;

	assert(this->sock != INVALID_SOCKET);

	if (!this->HasClientQuit()) {
		Debug(net, 3, "Closed client connection {}", this->client_id);

		this->SendPackets(true);

		/* Wait a number of ticks so our leave message can reach the server.
		 * This is especially needed for Windows servers as they seem to get
		 * the "socket is closed" message before receiving our leave message,
		 * which would trigger the server to close the connection as well. */
		CSleep(3 * MILLISECONDS_PER_TICK);
	}

	this->DeferDeletion();

	return status;
}

/**
 * Handle an error coming from the client side.
 * @param res The "error" that happened.
 */
void ClientNetworkGameSocketHandler::ClientError(NetworkRecvStatus res)
{
	if (this->IsPendingDeletion()) return;

	/* First, send a CLIENT_ERROR to the server, so it knows we are
	 *  disconnected (and why!) */
	NetworkErrorCode errorno;

	/* We just want to close the connection.. */
	if (res == NETWORK_RECV_STATUS_CLOSE_QUERY) {
		this->NetworkSocketHandler::MarkClosed();
		this->CloseConnection(res);
		_networking = false;

		CloseWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);
		return;
	}

	switch (res) {
		case NETWORK_RECV_STATUS_DESYNC:          errorno = NETWORK_ERROR_DESYNC; break;
		case NETWORK_RECV_STATUS_SAVEGAME:        errorno = NETWORK_ERROR_SAVEGAME_FAILED; break;
		case NETWORK_RECV_STATUS_NEWGRF_MISMATCH: errorno = NETWORK_ERROR_NEWGRF_MISMATCH; break;
		default:                                  errorno = NETWORK_ERROR_GENERAL; break;
	}

	if (res == NETWORK_RECV_STATUS_SERVER_ERROR || res == NETWORK_RECV_STATUS_SERVER_FULL ||
			res == NETWORK_RECV_STATUS_SERVER_BANNED) {
		/* This means the server closed the connection. Emergency save is
		 * already created if this was appropriate during handling of the
		 * disconnect. */
		this->CloseConnection(res);
	} else {
		/* This means we as client made a boo-boo. */
		SendError(errorno);

		/* Close connection before we make an emergency save, as the save can
		 * take a bit of time; better that the server doesn't stall while we
		 * are doing the save, and already disconnects us. */
		this->CloseConnection(res);
		ClientNetworkEmergencySave();
	}

	CloseWindowById(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);

	if (_game_mode != GM_MENU) _switch_mode = SM_MENU;
	_networking = false;
}


/**
 * Check whether we received/can send some data from/to the server and
 * when that's the case handle it appropriately.
 * @return true when everything went okay.
 */
/* static */ bool ClientNetworkGameSocketHandler::Receive()
{
	if (my_client->CanSendReceive()) {
		NetworkRecvStatus res = my_client->ReceivePackets();
		if (res != NETWORK_RECV_STATUS_OKAY) {
			/* The client made an error of which we can not recover.
			 * Close the connection and drop back to the main menu. */
			my_client->ClientError(res);
			return false;
		}
	}
	return _networking;
}

/** Send the packets of this socket handler. */
/* static */ void ClientNetworkGameSocketHandler::Send()
{
	my_client->SendPackets();
	if (my_client != nullptr) my_client->CheckConnection();
}

/**
 * Actual game loop for the client.
 * @return Whether everything went okay, or not.
 */
/* static */ bool ClientNetworkGameSocketHandler::GameLoop()
{
	_frame_counter++;

	NetworkExecuteLocalCommandQueue();

	StateGameLoop();

	citymania::HandleNextClientFrame();

	/* Check if we are in sync! */
	if (_sync_frame != 0) {
		if (_sync_frame == _frame_counter) {
#ifdef NETWORK_SEND_DOUBLE_SEED
			if (_sync_seed_1 != _random.state[0] || _sync_seed_2 != _random.state[1]) {
#else
			if (_sync_seed_1 != _random.state[0]) {
#endif
				ShowNetworkError(STR_NETWORK_ERROR_DESYNC);
				Debug(desync, 1, "sync_err: {:08x}; {:02x}", TimerGameEconomy::date, TimerGameEconomy::date_fract);
				Debug(net, 0, "Sync error detected");
				my_client->ClientError(NETWORK_RECV_STATUS_DESYNC);
				return false;
			}

			/* If this is the first time we have a sync-frame, we
			 *   need to let the server know that we are ready and at the same
			 *   frame as it is.. so we can start playing! */
			if (_network_first_time) {
				_network_first_time = false;
				SendAck();
			}

			_sync_frame = 0;
		} else if (_sync_frame < _frame_counter) {
			Debug(net, 1, "Missed frame for sync-test: {} / {}", _sync_frame, _frame_counter);
			_sync_frame = 0;
		}
	}

	return true;
}


/** Our client's connection. */
ClientNetworkGameSocketHandler * ClientNetworkGameSocketHandler::my_client = nullptr;

/** Last frame we performed an ack. */
static uint32_t last_ack_frame;

/** Maximum number of companies of the currently joined server. */
static uint8_t _network_server_max_companies;
/** The current name of the server you are on. */
std::string _network_server_name;

/** Information about the game to join to. */
NetworkJoinInfo _network_join;

/***********
 * Sending functions
 ************/

/** Tell the server we would like to join. */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendJoin()
{
	Debug(net, 9, "Client::SendJoin()");

	Debug(net, 9, "Client::status = JOIN");
	my_client->status = STATUS_JOIN;
	Debug(net, 9, "Client::join_status = AUTHORIZING");
	_network_join_status = NETWORK_JOIN_STATUS_AUTHORIZING;
	SetWindowDirty(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_JOIN);
	p->Send_string(GetNetworkRevisionString());
	p->Send_uint32(_openttd_newgrf_version);
	my_client->SendPacket(std::move(p));

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::SendIdentify()
{
	Debug(net, 9, "Client::SendIdentify()");

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_IDENTIFY);
	p->Send_string(_settings_client.network.client_name); // Client name
	p->Send_uint8 (_network_join.company);     // PlayAs
	p->Send_uint8 (citymania::GetAvailableLoadFormats());  // Compressnion formats that we can decompress
	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/** Tell the server we got all the NewGRFs. */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendNewGRFsOk()
{
	Debug(net, 9, "Client::SendNewGRFsOk()");

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_NEWGRFS_CHECKED);
	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Set the game password as requested.
 * @param password The game password.
 */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendAuthResponse()
{
	Debug(net, 9, "Client::SendAuthResponse()");

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_AUTH_RESPONSE);
	my_client->authentication_handler->SendResponse(*p);
	my_client->SendPacket(std::move(p));

	return NETWORK_RECV_STATUS_OKAY;
}

/** Request the map from the server. */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendGetMap()
{
	Debug(net, 9, "Client::SendGetMap()");

	Debug(net, 9, "Client::status = MAP_WAIT");
	my_client->status = STATUS_MAP_WAIT;

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_GETMAP);
	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/** Tell the server we received the complete map. */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendMapOk()
{
	Debug(net, 9, "Client::SendMapOk()");

	Debug(net, 9, "Client::status = ACTIVE");
	my_client->status = STATUS_ACTIVE;

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_MAP_OK);
	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/** Send an acknowledgement from the server's ticks. */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendAck()
{
	Debug(net, 9, "Client::SendAck()");

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_ACK);

	p->Send_uint32(_frame_counter);
	p->Send_uint8 (my_client->token);
	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Send a command to the server.
 * @param cp The command to send.
 */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendCommand(const CommandPacket &cp)
{
	Debug(net, 9, "Client::SendCommand(): cmd={}", cp.cmd);

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_COMMAND);
	my_client->NetworkGameSocketHandler::SendCommand(*p, cp);

	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/** Send a chat-packet over the network */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendChat(NetworkAction action, DestType type, int dest, const std::string &msg, int64_t data)
{
	Debug(net, 9, "Client::SendChat(): action={}, type={}, dest={}", action, type, dest);

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_CHAT);

	p->Send_uint8 (action);
	p->Send_uint8 (type);
	p->Send_uint32(dest);
	p->Send_string(msg);
	p->Send_uint64(data);

	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/** Send an error-packet over the network */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendError(NetworkErrorCode errorno)
{
	Debug(net, 9, "Client::SendError(): errorno={}", errorno);

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_ERROR);

	p->Send_uint8(errorno);
	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Tell the server that we like to change the name of the client.
 * @param name The new name.
 */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendSetName(const std::string &name)
{
	Debug(net, 9, "Client::SendSetName()");

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_SET_NAME);

	p->Send_string(name);
	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Tell the server we would like to quit.
 */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendQuit()
{
	Debug(net, 9, "Client::SendQuit()");

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_QUIT);

	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Send a console command.
 * @param pass The password for the remote command.
 * @param command The actual command.
 */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendRCon(const std::string &pass, const std::string &command)
{
	Debug(net, 9, "Client::SendRCon()");

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_RCON);
	p->Send_string(pass);
	p->Send_string(command);
	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Ask the server to move us.
 * @param company The company to move to.
 */
NetworkRecvStatus ClientNetworkGameSocketHandler::SendMove(CompanyID company)
{
	Debug(net, 9, "Client::SendMove(): company={}", company);

	auto p = std::make_unique<Packet>(my_client, PACKET_CLIENT_MOVE);
	p->Send_uint8(company);
	my_client->SendPacket(std::move(p));
	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Check whether the client is actually connected (and in the game).
 * @return True when the client is connected.
 */
bool ClientNetworkGameSocketHandler::IsConnected()
{
	return my_client != nullptr && my_client->status == STATUS_ACTIVE;
}


/***********
 * Receiving functions
 ************/

extern bool SafeLoad(const std::string &filename, SaveLoadOperation fop, DetailedFileType dft, GameMode newgm, Subdirectory subdir, std::shared_ptr<struct LoadFilter> lf);

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_FULL(Packet &)
{
	Debug(net, 9, "Client::Receive_SERVER_FULL()");

	/* We try to join a server which is full */
	ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_SERVER_FULL), {}, WL_CRITICAL);

	return NETWORK_RECV_STATUS_SERVER_FULL;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_BANNED(Packet &)
{
	Debug(net, 9, "Client::Receive_SERVER_BANNED()");

	/* We try to join a server where we are banned */
	ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_SERVER_BANNED), {}, WL_CRITICAL);

	return NETWORK_RECV_STATUS_SERVER_BANNED;
}

/* This packet contains info about the client (playas and name)
 *  as client we save this in NetworkClientInfo, linked via 'client_id'
 *  which is always an unique number on a server. */
NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_CLIENT_INFO(Packet &p)
{
	NetworkClientInfo *ci;
	ClientID client_id = (ClientID)p.Recv_uint32();
	CompanyID playas = (CompanyID)p.Recv_uint8();

	Debug(net, 9, "Client::Receive_SERVER_CLIENT_INFO(): client_id={}, playas={}", client_id, playas);

	std::string name = p.Recv_string(NETWORK_NAME_LENGTH);
	std::string public_key = p.Recv_string(NETWORK_PUBLIC_KEY_LENGTH);

	if (this->status < STATUS_AUTHORIZED) return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	if (this->HasClientQuit()) return NETWORK_RECV_STATUS_CLIENT_QUIT;
	/* The server validates the name when receiving it from clients, so when it is wrong
	 * here something went really wrong. In the best case the packet got malformed on its
	 * way too us, in the worst case the server is broken or compromised. */
	if (!NetworkIsValidClientName(name)) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	ci = NetworkClientInfo::GetByClientID(client_id);
	if (ci != nullptr) {
		if (playas == ci->client_playas && name.compare(ci->client_name) != 0) {
			/* Client name changed, display the change */
			NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, CC_DEFAULT, false, ci->client_name, name);
		} else if (playas != ci->client_playas) {
			/* The client changed from client-player..
			 * Do not display that for now */
		}

		/* Make sure we're in the company the server tells us to be in,
		 * for the rare case that we get moved while joining. */
		if (client_id == _network_own_client_id) SetLocalCompany(!Company::IsValidID(playas) ? COMPANY_SPECTATOR : playas);

		ci->client_playas = playas;
		ci->client_name = std::move(name);
		ci->public_key = std::move(public_key);

		citymania::SetClientListDirty();
		InvalidateWindowData(WC_CLIENT_LIST, 0);
		InvalidateWindowClassesData(WC_WATCH_COMPANY, 0);
		SetWindowClassesDirty(WC_WATCH_COMPANY);
		InvalidateWindowData(WC_WATCH_COMPANYA, ci->client_id, 1);
		SetWindowClassesDirty(WC_WATCH_COMPANYA);

		return NETWORK_RECV_STATUS_OKAY;
	}

	/* There are at most as many ClientInfo as ClientSocket objects in a
	 * server. Having more info than a server can have means something
	 * has gone wrong somewhere, i.e. the server has more info than it
	 * has actual clients. That means the server is feeding us an invalid
	 * state. So, bail out! This server is broken. */
	if (!NetworkClientInfo::CanAllocateItem()) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	/* We don't have this client_id yet, find an empty client_id, and put the data there */
	ci = new NetworkClientInfo(client_id);
	ci->client_playas = playas;
	if (client_id == _network_own_client_id) this->SetInfo(ci);

	ci->client_name = std::move(name);
	ci->public_key = std::move(public_key);

	citymania::SetClientListDirty();
	InvalidateWindowData(WC_CLIENT_LIST, 0);
	InvalidateWindowClassesData(WC_WATCH_COMPANY, 0);
	SetWindowClassesDirty(WC_WATCH_COMPANY);
	InvalidateWindowData(WC_WATCH_COMPANYA, ci->client_id, 1);
	SetWindowClassesDirty(WC_WATCH_COMPANYA);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_ERROR(Packet &p)
{
	static const StringID network_error_strings[] = {
		STR_NETWORK_ERROR_LOSTCONNECTION,      // NETWORK_ERROR_GENERAL
		STR_NETWORK_ERROR_LOSTCONNECTION,      // NETWORK_ERROR_DESYNC
		STR_NETWORK_ERROR_LOSTCONNECTION,      // NETWORK_ERROR_SAVEGAME_FAILED
		STR_NETWORK_ERROR_LOSTCONNECTION,      // NETWORK_ERROR_CONNECTION_LOST
		STR_NETWORK_ERROR_LOSTCONNECTION,      // NETWORK_ERROR_ILLEGAL_PACKET
		STR_NETWORK_ERROR_LOSTCONNECTION,      // NETWORK_ERROR_NEWGRF_MISMATCH
		STR_NETWORK_ERROR_SERVER_ERROR,        // NETWORK_ERROR_NOT_AUTHORIZED
		STR_NETWORK_ERROR_SERVER_ERROR,        // NETWORK_ERROR_NOT_EXPECTED
		STR_NETWORK_ERROR_WRONG_REVISION,      // NETWORK_ERROR_WRONG_REVISION
		STR_NETWORK_ERROR_LOSTCONNECTION,      // NETWORK_ERROR_NAME_IN_USE
		STR_NETWORK_ERROR_WRONG_PASSWORD,      // NETWORK_ERROR_WRONG_PASSWORD
		STR_NETWORK_ERROR_SERVER_ERROR,        // NETWORK_ERROR_COMPANY_MISMATCH
		STR_NETWORK_ERROR_KICKED,              // NETWORK_ERROR_KICKED
		STR_NETWORK_ERROR_CHEATER,             // NETWORK_ERROR_CHEATER
		STR_NETWORK_ERROR_SERVER_FULL,         // NETWORK_ERROR_FULL
		STR_NETWORK_ERROR_TOO_MANY_COMMANDS,   // NETWORK_ERROR_TOO_MANY_COMMANDS
		STR_NETWORK_ERROR_TIMEOUT_PASSWORD,    // NETWORK_ERROR_TIMEOUT_PASSWORD
		STR_NETWORK_ERROR_TIMEOUT_COMPUTER,    // NETWORK_ERROR_TIMEOUT_COMPUTER
		STR_NETWORK_ERROR_TIMEOUT_MAP,         // NETWORK_ERROR_TIMEOUT_MAP
		STR_NETWORK_ERROR_TIMEOUT_JOIN,        // NETWORK_ERROR_TIMEOUT_JOIN
		STR_NETWORK_ERROR_INVALID_CLIENT_NAME, // NETWORK_ERROR_INVALID_CLIENT_NAME
		STR_NETWORK_ERROR_NOT_ON_ALLOW_LIST,   // NETWORK_ERROR_NOT_ON_ALLOW_LIST
		STR_NETWORK_ERROR_SERVER_ERROR,        // NETWORK_ERROR_NO_AUTHENTICATION_METHOD_AVAILABLE
	};
	static_assert(lengthof(network_error_strings) == NETWORK_ERROR_END);

	NetworkErrorCode error = (NetworkErrorCode)p.Recv_uint8();

	Debug(net, 9, "Client::Receive_SERVER_ERROR(): error={}", error);

	StringID err = STR_NETWORK_ERROR_LOSTCONNECTION;
	if (error < (ptrdiff_t)lengthof(network_error_strings)) err = network_error_strings[error];
	/* In case of kicking a client, we assume there is a kick message in the packet if we can read one byte */
	if (error == NETWORK_ERROR_KICKED && p.CanReadFromPacket(1)) {
		ShowErrorMessage(GetEncodedString(err),
			GetEncodedString(STR_NETWORK_ERROR_KICK_MESSAGE, p.Recv_string(NETWORK_CHAT_LENGTH)),
			WL_CRITICAL);
	} else {
		ShowErrorMessage(GetEncodedString(err), {}, WL_CRITICAL);
	}

	/* Perform an emergency save if we had already entered the game */
	if (this->status == STATUS_ACTIVE) ClientNetworkEmergencySave();

	return NETWORK_RECV_STATUS_SERVER_ERROR;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_CHECK_NEWGRFS(Packet &p)
{
	if (this->status != STATUS_ENCRYPTED) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	uint grf_count = p.Recv_uint8();
	NetworkRecvStatus ret = NETWORK_RECV_STATUS_OKAY;

	Debug(net, 9, "Client::Receive_SERVER_CHECK_NEWGRFS(): grf_count={}", grf_count);

	/* Check all GRFs */
	for (; grf_count > 0; grf_count--) {
		GRFIdentifier c;
		DeserializeGRFIdentifier(p, c);

		/* Check whether we know this GRF */
		const GRFConfig *f = FindGRFConfig(c.grfid, FGCM_EXACT, &c.md5sum);
		if (f == nullptr) {
			/* We do not know this GRF, bail out of initialization */
			Debug(grf, 0, "NewGRF {:08X} not found; checksum {}", std::byteswap(c.grfid), FormatArrayAsHex(c.md5sum));
			ret = NETWORK_RECV_STATUS_NEWGRF_MISMATCH;
		}
	}

	if (ret == NETWORK_RECV_STATUS_OKAY) {
		/* Start receiving the map */
		return SendNewGRFsOk();
	}

	/* NewGRF mismatch, bail out */
	ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_NEWGRF_MISMATCH), {}, WL_CRITICAL);
	return ret;
}

class ClientGamePasswordRequestHandler : public NetworkAuthenticationPasswordRequestHandler {
	virtual void SendResponse() override { MyClient::SendAuthResponse(); }
	virtual void AskUserForPassword(std::shared_ptr<NetworkAuthenticationPasswordRequest> request) override
	{
		if (!_network_join.server_password.empty()) {
			request->Reply(_network_join.server_password);
		} else {
			ShowNetworkNeedPassword(std::move(request));
		}
	}
};

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_AUTH_REQUEST(Packet &p)
{
	if (this->status != STATUS_JOIN && this->status != STATUS_AUTH_GAME) return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	Debug(net, 9, "Client::status = AUTH_GAME");
	this->status = STATUS_AUTH_GAME;

	Debug(net, 9, "Client::Receive_SERVER_AUTH_REQUEST()");

	if (this->authentication_handler == nullptr) {
		this->authentication_handler = NetworkAuthenticationClientHandler::Create(std::make_shared<ClientGamePasswordRequestHandler>(),
				_settings_client.network.client_secret_key, _settings_client.network.client_public_key);
	}
	switch (this->authentication_handler->ReceiveRequest(p)) {
		case NetworkAuthenticationClientHandler::RequestResult::ReadyForResponse:
			return SendAuthResponse();

		case NetworkAuthenticationClientHandler::RequestResult::AwaitUserInput:
			return NETWORK_RECV_STATUS_OKAY;

		case NetworkAuthenticationClientHandler::RequestResult::Invalid:
		default:
			return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	}
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_ENABLE_ENCRYPTION(Packet &p)
{
	if (this->status != STATUS_AUTH_GAME || this->authentication_handler == nullptr) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	Debug(net, 9, "Client::Receive_SERVER_ENABLE_ENCRYPTION()");

	if (!this->authentication_handler->ReceiveEnableEncryption(p)) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	this->receive_encryption_handler = this->authentication_handler->CreateServerToClientEncryptionHandler();
	this->send_encryption_handler = this->authentication_handler->CreateClientToServerEncryptionHandler();
	this->authentication_handler = nullptr;

	Debug(net, 9, "Client::status = ENCRYPTED");
	this->status = STATUS_ENCRYPTED;

	return this->SendIdentify();
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_WELCOME(Packet &p)
{
	if (this->status < STATUS_ENCRYPTED || this->status >= STATUS_AUTHORIZED) return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	Debug(net, 9, "Client::status = AUTHORIZED");
	this->status = STATUS_AUTHORIZED;

	_network_own_client_id = (ClientID)p.Recv_uint32();

	Debug(net, 9, "Client::Receive_SERVER_WELCOME(): client_id={}", _network_own_client_id);

	/* Start receiving the map */
	return SendGetMap();
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_WAIT(Packet &p)
{
	/* We set the internal wait state when requesting the map. */
	if (this->status != STATUS_MAP_WAIT) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	Debug(net, 9, "Client::Receive_SERVER_WAIT()");

	/* But... only now we set the join status to waiting, instead of requesting. */
	Debug(net, 9, "Client::join_status = WAITING");
	_network_join_status = NETWORK_JOIN_STATUS_WAITING;
	_network_join_waiting = p.Recv_uint8();
	SetWindowDirty(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_MAP_BEGIN(Packet &p)
{
	if (this->status < STATUS_AUTHORIZED || this->status >= STATUS_MAP) return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	Debug(net, 9, "Client::status = MAP");
	this->status = STATUS_MAP;

	if (this->savegame != nullptr) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	this->savegame = std::make_shared<PacketReader>();

	_frame_counter = _frame_counter_server = _frame_counter_max = p.Recv_uint32();
	citymania::InitCommandQueue();

	Debug(net, 9, "Client::Receive_SERVER_MAP_BEGIN(): frame_counter={}", _frame_counter);

	_network_join_bytes = 0;
	_network_join_bytes_total = 0;

	Debug(net, 9, "Client::join_status = DOWNLOADING");
	_network_join_status = NETWORK_JOIN_STATUS_DOWNLOADING;
	SetWindowDirty(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_MAP_SIZE(Packet &p)
{
	if (this->status != STATUS_MAP) return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	if (this->savegame == nullptr) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	_network_join_bytes_total = p.Recv_uint32();
	SetWindowDirty(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);

	Debug(net, 9, "Client::Receive_SERVER_MAP_SIZE(): bytes_total={}", _network_join_bytes_total);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_MAP_DATA(Packet &p)
{
	if (this->status != STATUS_MAP) return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	if (this->savegame == nullptr) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	/* We are still receiving data, put it to the file */
	this->savegame->AddPacket(p);

	_network_join_bytes = static_cast<uint32_t>(this->savegame->buffer.size());
	SetWindowDirty(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_MAP_DONE(Packet &)
{
	if (this->status != STATUS_MAP) return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	if (this->savegame == nullptr) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	Debug(net, 9, "Client::Receive_SERVER_MAP_DONE()");

	Debug(net, 9, "Client::join_status = PROCESSING");
	_network_join_status = NETWORK_JOIN_STATUS_PROCESSING;
	SetWindowDirty(WC_NETWORK_STATUS_WINDOW, WN_NETWORK_STATUS_WINDOW_JOIN);

	this->savegame->Reset();

	/* The map is done downloading, load it */
	ClearErrorMessages();

	/* Set the abstract filetype. This is read during savegame load. */
	_file_to_saveload.SetMode(SLO_LOAD, FT_SAVEGAME, DFT_GAME_FILE);

	bool load_success = SafeLoad({}, SLO_LOAD, DFT_GAME_FILE, GM_NORMAL, NO_DIRECTORY, this->savegame);
	this->savegame = nullptr;

	/* Long savegame loads shouldn't affect the lag calculation! */
	this->last_packet = std::chrono::steady_clock::now();

	if (!load_success) {
		ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_SAVEGAMEERROR), {}, WL_CRITICAL);
		return NETWORK_RECV_STATUS_SAVEGAME;
	}
	/* If the savegame has successfully loaded, ALL windows have been removed,
	 * only toolbar/statusbar and gamefield are visible */

	/* Say we received the map and loaded it correctly! */
	SendMapOk();

	/* As we skipped switch-mode, update the time we "switched". */
	_game_session_stats.start_time = std::chrono::steady_clock::now();
	_game_session_stats.savegame_size = std::nullopt;
	_game_session_stats.cm = {};

	ShowClientList();

	/* New company/spectator (invalid company) or company we want to join is not active
	 * Switch local company to spectator and await the server's judgement */
	if (_network_join.company == COMPANY_NEW_COMPANY || !Company::IsValidID(_network_join.company)) {
		SetLocalCompany(COMPANY_SPECTATOR);

		if (_network_join.company != COMPANY_SPECTATOR) {
			/* We have arrived and ready to start playing; send a command to make a new company;
			 * the server will give us a client-id and let us in */
			Debug(net, 9, "Client::join_status = REGISTERING");
			_network_join_status = NETWORK_JOIN_STATUS_REGISTERING;
			ShowJoinStatusWindow();
			Command<CMD_COMPANY_CTRL>::Post(CCA_NEW, CompanyID::Invalid(), CRR_NONE, _network_own_client_id);
		}
	} else {
		/* take control over an existing company */
		SetLocalCompany(_network_join.company);
	}

	SocialIntegration::EventEnterMultiplayer(Map::SizeX(), Map::SizeY());

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_FRAME(Packet &p)
{
	if (this->status != STATUS_ACTIVE) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	_frame_counter_server = p.Recv_uint32();
	_frame_counter_max = p.Recv_uint32();
#ifdef ENABLE_NETWORK_SYNC_EVERY_FRAME
	/* Test if the server supports this option
	 *  and if we are at the frame the server is */
#ifdef NETWORK_SEND_DOUBLE_SEED
	if (p.CanReadFromPacket(sizeof(uint32_t) + sizeof(uint32_t))) {
#else
	if (p.CanReadFromPacket(sizeof(uint32_t))) {
#endif
		_sync_frame = _frame_counter_server;
		_sync_seed_1 = p.Recv_uint32();
#ifdef NETWORK_SEND_DOUBLE_SEED
		_sync_seed_2 = p.Recv_uint32();
#endif
	}
#endif
	/* Receive the token. */
	if (p.CanReadFromPacket(sizeof(uint8_t))) this->token = p.Recv_uint8();

	/* Let the server know that we received this frame correctly
	 *  We do this only once per day, to save some bandwidth ;) */
	if (!_network_first_time && last_ack_frame < _frame_counter) {
		last_ack_frame = _frame_counter + Ticks::DAY_TICKS;
		Debug(net, 7, "Sent ACK at {}", _frame_counter);
		SendAck();
	}

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_SYNC(Packet &p)
{
	if (this->status != STATUS_ACTIVE) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	_sync_frame = p.Recv_uint32();
	_sync_seed_1 = p.Recv_uint32();
#ifdef NETWORK_SEND_DOUBLE_SEED
	_sync_seed_2 = p.Recv_uint32();
#endif

	Debug(net, 9, "Client::Receive_SERVER_SYNC(): sync_frame={}, sync_seed_1={}", _sync_frame, _sync_seed_1);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_COMMAND(Packet &p)
{
	if (this->status != STATUS_ACTIVE) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	CommandPacket cp;
	const char *err = this->ReceiveCommand(p, cp);
	cp.frame    = p.Recv_uint32();
	cp.my_cmd   = p.Recv_bool();

	Debug(net, 9, "Client::Receive_SERVER_COMMAND(): cmd={}, frame={}", cp.cmd, cp.frame);

	if (err != nullptr) {
		IConsolePrint(CC_WARNING, "Dropping server connection due to {}.", err);
		return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	}

	this->incoming_queue.push_back(std::move(cp));

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_CHAT(Packet &p)
{
	if (this->status != STATUS_ACTIVE) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	std::string name;
	const NetworkClientInfo *ci = nullptr, *ci_to;

	NetworkAction action = (NetworkAction)p.Recv_uint8();
	ClientID client_id = (ClientID)p.Recv_uint32();
	bool self_send = p.Recv_bool();
	std::string msg = p.Recv_string(NETWORK_CHAT_LENGTH);
	int64_t data = p.Recv_uint64();

	Debug(net, 9, "Client::Receive_SERVER_CHAT(): action={}, client_id={}, self_send={}", action, client_id, self_send);

	ci_to = NetworkClientInfo::GetByClientID(client_id);
	if (ci_to == nullptr) return NETWORK_RECV_STATUS_OKAY;

	/* Did we initiate the action locally? */
	if (self_send) {
		switch (action) {
			case NETWORK_ACTION_CHAT_CLIENT:
				/* For speaking to client we need the client-name */
				name = ci_to->client_name;
				ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
				break;

			/* For speaking to company, we need the company-name */
			case NETWORK_ACTION_CHAT_COMPANY: {
				StringID str = Company::IsValidID(ci_to->client_playas) ? STR_COMPANY_NAME : STR_NETWORK_SPECTATORS;

				name = GetString(str, ci_to->client_playas);
				ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
				break;
			}

			default: return NETWORK_RECV_STATUS_MALFORMED_PACKET;
		}
	} else {
		/* Display message from somebody else */
		name = ci_to->client_name;
		ci = ci_to;
	}

	if (ci != nullptr) {
		if (msg.rfind("synccmuser", 0) == 0) SyncCMUser(msg);
		else NetworkTextMessage(action, GetDrawStringCompanyColour(ci->client_playas), self_send, name, msg, data);
	}
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_EXTERNAL_CHAT(Packet &p)
{
	if (this->status != STATUS_ACTIVE) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	std::string source = p.Recv_string(NETWORK_CHAT_LENGTH);
	TextColour colour = (TextColour)p.Recv_uint16();
	std::string user = p.Recv_string(NETWORK_CHAT_LENGTH);
	std::string msg = p.Recv_string(NETWORK_CHAT_LENGTH);

	Debug(net, 9, "Client::Receive_SERVER_EXTERNAL_CHAT(): source={}", source);

	if (!IsValidConsoleColour(colour)) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	NetworkTextMessage(NETWORK_ACTION_EXTERNAL_CHAT, colour, false, user, msg, source);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_ERROR_QUIT(Packet &p)
{
	if (this->status < STATUS_AUTHORIZED) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	ClientID client_id = (ClientID)p.Recv_uint32();

	Debug(net, 9, "Client::Receive_SERVER_ERROR_QUIT(): client_id={}", client_id);

	NetworkClientInfo *ci = NetworkClientInfo::GetByClientID(client_id);
	if (ci != nullptr) {
		NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, ci->client_name, "", GetNetworkErrorMsg((NetworkErrorCode)p.Recv_uint8()));
		delete ci;
	}

	citymania::SetClientListDirty();
	InvalidateWindowData(WC_CLIENT_LIST, 0);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_QUIT(Packet &p)
{
	if (this->status < STATUS_AUTHORIZED) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	ClientID client_id = (ClientID)p.Recv_uint32();

	Debug(net, 9, "Client::Receive_SERVER_QUIT(): client_id={}", client_id);

	NetworkClientInfo *ci = NetworkClientInfo::GetByClientID(client_id);
	if (ci != nullptr) {
		NetworkTextMessage(NETWORK_ACTION_LEAVE, CC_DEFAULT, false, ci->client_name, "", STR_NETWORK_MESSAGE_CLIENT_LEAVING);
		delete ci;
	} else {
		Debug(net, 1, "Unknown client ({}) is leaving the game", client_id);
	}

	citymania::SetClientListDirty();
	InvalidateWindowData(WC_CLIENT_LIST, 0);
	InvalidateWindowClassesData( WC_WATCH_COMPANYA, 0 );
	SetWindowClassesDirty( WC_WATCH_COMPANYA );

	/* If we come here it means we could not locate the client.. strange :s */
	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_JOIN(Packet &p)
{
	if (this->status < STATUS_AUTHORIZED) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	ClientID client_id = (ClientID)p.Recv_uint32();

	Debug(net, 9, "Client::Receive_SERVER_JOIN(): client_id={}", client_id);

	NetworkClientInfo *ci = NetworkClientInfo::GetByClientID(client_id);
	if (ci != nullptr) {
		NetworkTextMessage(NETWORK_ACTION_JOIN, CC_DEFAULT, false, ci->client_name);
	}

	citymania::SetClientListDirty();
	InvalidateWindowData(WC_CLIENT_LIST, 0);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_SHUTDOWN(Packet &)
{
	Debug(net, 9, "Client::Receive_SERVER_SHUTDOWN()");

	/* Only when we're trying to join we really
	 * care about the server shutting down. */
	if (this->status >= STATUS_JOIN) {
		ShowErrorMessage(GetEncodedString(STR_NETWORK_MESSAGE_SERVER_SHUTDOWN), {}, WL_CRITICAL);
	}

	if (this->status == STATUS_ACTIVE) ClientNetworkEmergencySave();

	return NETWORK_RECV_STATUS_SERVER_ERROR;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_NEWGAME(Packet &)
{
	Debug(net, 9, "Client::Receive_SERVER_NEWGAME()");

	/* Only when we're trying to join we really
	 * care about the server shutting down. */
	if (this->status >= STATUS_JOIN) {
		/* To throttle the reconnects a bit, every clients waits its
		 * Client ID modulo 16 + 1 (value 0 means no reconnect).
		 * This way reconnects should be spread out a bit. */
		_network_reconnect = _network_own_client_id % 16 + 1;
		ShowErrorMessage(GetEncodedString(STR_NETWORK_MESSAGE_SERVER_REBOOT), {}, WL_CRITICAL);
	}

	if (this->status == STATUS_ACTIVE) ClientNetworkEmergencySave();

	return NETWORK_RECV_STATUS_SERVER_ERROR;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_RCON(Packet &p)
{
	if (this->status < STATUS_AUTHORIZED) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	Debug(net, 9, "Client::Receive_SERVER_RCON()");

	TextColour colour_code = (TextColour)p.Recv_uint16();
	if (!IsValidConsoleColour(colour_code)) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	std::string rcon_out = p.Recv_string(NETWORK_RCONCOMMAND_LENGTH);

	IConsolePrint(colour_code, rcon_out);

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_MOVE(Packet &p)
{
	if (this->status < STATUS_AUTHORIZED) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	/* Nothing more in this packet... */
	ClientID client_id   = (ClientID)p.Recv_uint32();
	CompanyID company_id = (CompanyID)p.Recv_uint8();

	Debug(net, 9, "Client::Receive_SERVER_MOVE(): client_id={}, comapny_id={}", client_id, company_id);

	if (client_id == 0) {
		/* definitely an invalid client id, debug message and do nothing. */
		Debug(net, 1, "Received invalid client index = 0");
		return NETWORK_RECV_STATUS_MALFORMED_PACKET;
	}

	const NetworkClientInfo *ci = NetworkClientInfo::GetByClientID(client_id);
	/* Just make sure we do not try to use a client_index that does not exist */
	if (ci == nullptr) return NETWORK_RECV_STATUS_OKAY;

	/* if not valid player, force spectator, else check player exists */
	if (!Company::IsValidID(company_id)) company_id = COMPANY_SPECTATOR;

	if (client_id == _network_own_client_id) {
		SetLocalCompany(company_id);
	}
	InvalidateWindowClassesData( WC_WATCH_COMPANYA, 0 );
	SetWindowClassesDirty( WC_WATCH_COMPANYA );

	return NETWORK_RECV_STATUS_OKAY;
}

NetworkRecvStatus ClientNetworkGameSocketHandler::Receive_SERVER_CONFIG_UPDATE(Packet &p)
{
	if (this->status < STATUS_ACTIVE) return NETWORK_RECV_STATUS_MALFORMED_PACKET;

	_network_server_max_companies = p.Recv_uint8();
	_network_server_name = p.Recv_string(NETWORK_NAME_LENGTH);

	InvalidateWindowData(WC_CLIENT_LIST, 0);

	Debug(net, 9, "Client::Receive_SERVER_CONFIG_UPDATE(): max_companies={}", _network_server_max_companies);

	return NETWORK_RECV_STATUS_OKAY;
}

/**
 * Check the connection's state, i.e. is the connection still up?
 */
void ClientNetworkGameSocketHandler::CheckConnection()
{
	/* Only once we're authorized we can expect a steady stream of packets. */
	if (this->status < STATUS_AUTHORIZED) return;

	/* 5 seconds are roughly twice the server's "you're slow" threshold (1 game day). */
	std::chrono::steady_clock::duration lag = std::chrono::steady_clock::now() - this->last_packet;
	if (lag < std::chrono::seconds(5)) return;

	/* 20 seconds are (way) more than 4 game days after which
	 * the server will forcefully disconnect you. */
	if (lag > std::chrono::seconds(20)) {
		this->NetworkGameSocketHandler::CloseConnection();
		return;
	}

	/* Prevent showing the lag message every tick; just update it when needed. */
	static std::chrono::steady_clock::duration last_lag = {};
	if (std::chrono::duration_cast<std::chrono::seconds>(last_lag) == std::chrono::duration_cast<std::chrono::seconds>(lag)) return;

	last_lag = lag;
	ShowErrorMessage(
		GetEncodedString(STR_NETWORK_ERROR_CLIENT_GUI_LOST_CONNECTION_CAPTION),
		GetEncodedString(STR_NETWORK_ERROR_CLIENT_GUI_LOST_CONNECTION, std::chrono::duration_cast<std::chrono::seconds>(lag).count()),
		WL_INFO);
}


/** Is called after a client is connected to the server */
void NetworkClient_Connected()
{
	/* Set the frame-counter to 0 so nothing happens till we are ready */
	_frame_counter = 0;
	_frame_counter_server = 0;
	last_ack_frame = 0;

	Debug(net, 9, "Client::NetworkClient_Connected()");

	/* Request the game-info */
	MyClient::SendJoin();
}

/**
 * Send a remote console command.
 * @param password The password.
 * @param command The command to execute.
 */
void NetworkClientSendRcon(const std::string &password, const std::string &command)
{
	MyClient::SendRCon(password, command);
}

/**
 * Notify the server of this client wanting to be moved to another company.
 * @param company_id id of the company the client wishes to be moved to.
 */
void NetworkClientRequestMove(CompanyID company_id)
{
	MyClient::SendMove(company_id);
}

/**
 * Move the clients of a company to the spectators.
 * @param cid The company to move the clients of.
 */
void NetworkClientsToSpectators(CompanyID cid)
{
	Backup<CompanyID> cur_company(_current_company);
	/* If our company is changing owner, go to spectators */
	if (cid == _local_company) SetLocalCompany(COMPANY_SPECTATOR);

	for (NetworkClientInfo *ci : NetworkClientInfo::Iterate()) {
		if (ci->client_playas != cid) continue;
		NetworkTextMessage(NETWORK_ACTION_COMPANY_SPECTATOR, CC_DEFAULT, false, ci->client_name);
		ci->client_playas = COMPANY_SPECTATOR;
	}

	cur_company.Restore();
}

/**
 * Check whether the given client name is deemed valid for use in network games.
 * An empty name (null or '') is not valid as that is essentially no name at all.
 * A name starting with white space is not valid for tab completion purposes.
 * @param client_name The client name to check for validity.
 * @return True iff the name is valid.
 */
bool NetworkIsValidClientName(const std::string_view client_name)
{
	if (client_name.empty()) return false;
	if (client_name[0] == ' ') return false;
	return true;
}

/**
 * Trim the given client name in place, i.e. remove leading and trailing spaces.
 * After the trim check whether the client name is valid. A client name is valid
 * whenever the name is not empty and does not start with spaces. This check is
 * done via \c NetworkIsValidClientName.
 * When the client name is valid, this function returns true.
 * When the client name is not valid a GUI error message is shown telling the
 * user to set the client name and this function returns false.
 *
 * This function is not suitable for ensuring a valid client name at the server
 * as the error message will then be shown to the host instead of the client.
 * @param client_name The client name to validate. It will be trimmed of leading
 *                    and trailing spaces.
 * @return True iff the client name is valid.
 */
bool NetworkValidateClientName(std::string &client_name)
{
	StrTrimInPlace(client_name);
	if (NetworkIsValidClientName(client_name)) return true;

	ShowErrorMessage(GetEncodedString(STR_NETWORK_ERROR_BAD_PLAYER_NAME), {}, WL_ERROR);
	return false;
}

/**
 * Convenience method for NetworkValidateClientName on _settings_client.network.client_name.
 * It trims the client name and checks whether it is empty. When it is empty
 * an error message is shown to the GUI user.
 * See \c NetworkValidateClientName(char*) for details about the functionality.
 * @return True iff the client name is valid.
 */
bool NetworkValidateOurClientName()
{
	return NetworkValidateClientName(_settings_client.network.client_name);
}

/**
 * Send the server our name as callback from the setting.
 * @param newname The new client name.
 */
void NetworkUpdateClientName(const std::string &client_name)
{
	NetworkClientInfo *ci = NetworkClientInfo::GetByClientID(_network_own_client_id);
	if (ci == nullptr) return;

	/* Don't change the name if it is the same as the old name */
	if (client_name.compare(ci->client_name) != 0) {
		if (!_network_server) {
			MyClient::SendSetName(client_name);
		} else {
			/* Copy to a temporary buffer so no #n gets added after our name in the settings when there are duplicate names. */
			std::string temporary_name = client_name;
			if (NetworkMakeClientNameUnique(temporary_name)) {
				NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, CC_DEFAULT, false, ci->client_name, temporary_name);
				ci->client_name = std::move(temporary_name);
				NetworkUpdateClientInfo(CLIENT_ID_SERVER);
			}
		}
	}
}

/**
 * Send a chat message.
 * @param action The action associated with the message.
 * @param type The destination type.
 * @param dest The destination index, be it a company index or client id.
 * @param msg The actual message.
 * @param data Arbitrary extra data.
 */
void NetworkClientSendChat(NetworkAction action, DestType type, int dest, const std::string &msg, int64_t data)
{
	MyClient::SendChat(action, type, dest, msg, data);
}

/**
 * Tell whether the client has team members who they can chat to.
 * @param cio client to check members of.
 * @return true if there is at least one team member.
 */
bool NetworkClientPreferTeamChat(const NetworkClientInfo *cio)
{
	/* Only companies actually playing can speak to team. Eg spectators cannot */
	if (!_settings_client.gui.prefer_teamchat || !Company::IsValidID(cio->client_playas)) return false;

	for (const NetworkClientInfo *ci : NetworkClientInfo::Iterate()) {
		if (ci->client_playas == cio->client_playas && ci != cio) return true;
	}

	return false;
}

/**
 * Get the maximum number of companies that are allowed by the server.
 * @return The number of companies allowed.
 */
uint NetworkMaxCompaniesAllowed()
{
	return _network_server ? _settings_client.network.max_companies : _network_server_max_companies;
}

/**
 * Check if max_companies has been reached on the server (local check only).
 * @return true if the max value has been reached or exceeded, false otherwise.
 */
bool NetworkMaxCompaniesReached()
{
	return Company::GetNumItems() >= NetworkMaxCompaniesAllowed();
}

void SyncCMUser(const std::string &msg) {
	uint user_id, role;
	sscanf(msg.c_str() + 10, "%u %u", &user_id, &role);
	// _novarole = (role >= 50);
	Debug(net, 1, "CityMania user synchronized: %u %u", user_id, role);
}

