#include "flags.h"
#include "types.h"
#include "eeprom_address.h"
#include "display.h"
#include "eeprom.h"
#include "ir.h"
#include "friend.h"
#include "common.h"
#include "startup.h"

/* IR checksum: start at 2, sum big-endian 16-bit pairs in a 32-bit accumulator,
 * and fold the carry twice without complementing it. Treat header checksum
 * bytes as zero while summing; store the result low byte first. */
uint PacketChecksum(u8 *bytes, u32 length)
{
  u32 checksum = 2;
  u32 index = 0;

  while (index < length) {
    u8 value = *bytes++;

    if (index & 1) {
      checksum += value; /* odd index: low octet */
    } else {
      checksum += (uint)value << 8; /* even index: high octet */
    }
    ++index;
  }

  checksum = (checksum >> 16) + (uint)checksum;
  checksum += checksum >> 16;

  return checksum;
}

#include "iodefine.h"
#include "project.h"

/* Initialize both EEPROM cursors and reset transfer progress at the start of
 * each peer phase. */
#pragma inline(BeginBulkPhase)
static void BeginBulkPhase(u8 mode, uint source, uint destination, uint length)
{
  g_work.irc.work.bulkMode = mode;
  g_work.irc.work.bulkSourceEepromAddress = source;
  g_work.irc.work.bulkDestinationEepromAddress = destination;
  g_work.irc.work.bulkBytesRemaining = length;
  g_work.irc.work.bulkChunksCompleted = 0;
}

/* Motion, sound and infrared communication reuse this workspace. */
Workspace g_work;

#pragma interrupt(Sci3Interrupt(vect = 37))
void Sci3Interrupt(void)
{
}

/* Return the payload window that sits immediately after the 8-byte header. */
u8 *IrPayload(void)
{
  return (g_work.irc.packet + 8);
}

/* Configure the IRC GPIO direction and output levels. */
void IrInitPins(void)
{
  IO.TARGET_F088 = 3;
  IO.PDR3.BYTE = 1;
  IO.PCR3 = 5;
}

void IrTransmitByte(u8 value);

/* The eight-byte header contains a command, its parameter, a little-endian
 * checksum and four session-token bytes. Payload follows immediately. */
typedef struct {
  u8 command;
  u8 argument;
  u8 checksumLo;
  u8 checksumHi;
  u32 sessionToken;
} IrcHeader;

/* The post-transport dispatcher consumes the completed command from session
 * state. */
#define PW_IR_PACKET_HEADER_SIZE 8

/* Build the wire header with zero checksum bytes, checksum header and payload,
 * XOR-transmit the frame, wait for SCI3 TEND and a two-count Timer W gap, then
 * drain a pending receive byte. */
void SendPacket(u8 payloadLength, u8 command, u8 argument)
{
  IrcHeader *header;
  u16 checksum;
  u16 i;
  u16 t0;

  header = (IrcHeader *)g_work.irc.packet;
  header->command = command;
  header->argument = argument;
  header->sessionToken = g_work.irc.work.sessionToken;
  header->checksumLo = 0;
  header->checksumHi = 0;

  checksum = PacketChecksum(g_work.irc.packet, (payloadLength + 8u));
  header->checksumLo = checksum;
  header->checksumHi = (checksum >> 8);

  payloadLength += PW_IR_PACKET_HEADER_SIZE;
  i = 0;
  while (i < payloadLength) {
    IrTransmitByte(g_work.irc.packet[i]);
    i++;
  }

  while (SCI3.SSR3.BIT.TEND == 0) {
  }
  t0 = TW.TCNT;
  while ((TW.TCNT - t0) < 2) {
  }
  if (SCI3.SSR3.BIT.RDRF != 0) {
    g_work.irc.work.sci3RxDrainByte = SCI3.RDR3;
  }
}

/* Ungate SCI3, clear its status and mode, allow settling time, and enable the
 * IrDA transmit path. */
void IrConfigure(void)
{
  s8 settle;

  CKSTPR1.BYTE |= 0x40; /* SCI3 clock-stop bit 6 */
  SCI3.SPCR.BYTE = 1;
  SCI3.SSR3.BYTE &= 0x84;
  SCI3.SEMR.BYTE = 0;
  SCI3.SCR3.BYTE = 0;
  SCI3.SMR3.BYTE = 0;
  SCI3.BRR3 = 0;
  settle = 5;
  do {
  } while (--settle != 0);
  SCI3.SCR3.BYTE = 0x10;
  SCI3.IrCR.BYTE = 0x80;
  SCI3.SPCR.BYTE = 0x11;
  SCI3.SCR3.BYTE = 0x30;
}

/* Wait for SCI3 TDRE, XOR the byte with the transport mask, and write TDR3. */
void IrTransmitByte(u8 value)
{
  while (SCI3.SSR3.BIT.TDRE == 0) {
  }
  SCI3.TDR3 = (value ^ PW_IR_TRANSPORT_XOR);
}

void IrInit(void)
{
  IrInitPins();
}

/* SCI3/IrDA bring-up: configure the UART, settle PDR3 around four delay units,
 * ungated Timer W as a free-running stamp, then drain a stale RDR3. */
void IrHardwareStart(void)
{
  u8 ssr3;

  IrConfigure();
  LowClockDelay();
  LowClockDelay();
  IO.PDR3.BYTE = 0;
  LowClockDelay();
  LowClockDelay();
  CKSTPR2.BIT.TWCKSTP = 1;
  TW.TCRW.BYTE = ((TW.TCRW.BYTE & 0x8f) | 0x40);
  TW.TCRW.BIT.CCLR = 0;
  TW.TIERW.BIT.IMIEA = 0;
  TW.TMRW.BIT.CTS = 1;
  ssr3 = SCI3.SSR3.BYTE;
  SCI3.SSR3.BYTE = (ssr3 & 0xc4);
  if (SCI3.SSR3.BIT.RDRF != 0) {
    g_work.irc.work.sci3RxDrainByte = SCI3.RDR3;
  }
}

enum {
  IR_PHASE_PROBING = 1,
  IR_PHASE_REPLY_SENT = 2,
  IR_PHASE_INITIATOR = 3,
  IR_PHASE_RESPONDER = 4
};

#define IR_ACK 0xF8u
#define IR_RESPONSE 0xFAu
#define IR_CONNECT 0xFCu
#define IR_SHUTDOWN 0xF4u

#define PW_IR_RX_WINDOW_CAPACITY 0x88u
#define PW_IR_FRAME_GAP_TICKS 4
#define PW_IR_INACTIVITY_TICKS 0xc80
#define PW_IR_TIMEOUT_RETRY_LIMIT 0x14u
#define PW_IR_CHECKSUM_FAIL_LIMIT 0x14u
#define PW_IR_RETRY_JITTER_MASK 0x0Fu
#define PW_IR_RETRY_JITTER_TICKS 0x60u

#define PW_IRC_PEER_BULK_SEND_POKEMON_ICON 1
#define PW_IRC_PEER_BULK_RECV_POKEMON_ICON 2
#define PW_IRC_PEER_BULK_SEND_POKE_NAME 3
#define PW_IRC_PEER_BULK_RECV_POKE_NAME 4
#define PW_IRC_PEER_BULK_SEND_RECORD 5
#define PW_IRC_PEER_BULK_RECV_RECORD 6
#define PW_PEER_FRIEND_INFO_BYTES sizeof(PeerInfo)
#define PW_PEER_FRIEND_INFO_ADDRESS (EEPROM_PEER_IMAGE + 384 + 320)
#define PW_IR_BULK_MAXIMUM_CHUNK_LENGTH 0x80

typedef char IrcReceiveStorageHoldsMaximumFrame
    [sizeof(g_work.irc.packet) >= PW_IR_RX_WINDOW_CAPACITY ? 1 : -1];
typedef char IrcDecodeStorageHoldsMaximumChunk
    [sizeof(g_work.irc.eepromScratch) >= PW_IR_BULK_MAXIMUM_CHUNK_LENGTH ? 1
                                                                         : -1];
typedef char IrcHeaderMatchesPayloadOffset
    [sizeof(IrcHeader) == PW_IR_PACKET_HEADER_SIZE ? 1 : -1];
#define PW_PEER_POKE_NAME_ADDRESS                                              \
  (EEPROM_PEER_IMAGE + sizeof(((CourseResources *)0)->pokemonImage))

/* Start probing with a token from the PRNG state, clear session progress,
 * and send the one-byte connection probe. */
void IrBegin(void)
{
  IrHardwareStart();
  g_state.irResult = 0;
  g_work.irc.work.localSessionWord = g_state.randomState;
  g_work.irc.work.sessionToken = g_work.irc.work.localSessionWord;
  g_work.irc.work.phase = IR_PHASE_PROBING;
  g_work.irc.work.timeoutRetryCount = 0;
  g_work.irc.work.checksumFailureCount = 0;
  g_work.irc.work.completionAction = 0xff;
  g_work.irc.work.sessionFlags.bits.receivedAny = 0;
  g_state.irReceiveWindow = 0;
  g_work.irc.work.writeOnlyZeroByte = 0;
  g_state.irTimerStart = TW.TCNT;
  g_work.irc.work.bulkMode = 0;
  IrTransmitByte(IR_CONNECT);
}

void IrFinish(void);

/* Infrared-session view of the shared UI state. */

/* Expose the peer role as a shared byte-valued query. */
#pragma inline(IsPeerSender)
static u8 IsPeerSender(void)
{
  return g_ui.view.ir.role.bits.peerSender;
}

/* Compare the Pokemon's species, form, sex, and rare-color flag. */
/* Expose the IR decode buffer as a course-Pokemon record. */
#pragma inline(CoursePokemonBuffer)
static Pokemon *CoursePokemonBuffer(void)
{
  Pokemon *record;

  record = (Pokemon *)g_work.irc.eepromScratch;
  return record;
}

/* Both protocol transitions build the same outgoing record. */
#pragma inline(BuildPeerInfo)
static void BuildPeerInfo(void)
{
  PeerInfo *friend;
  Pokemon *pokemon;
  s16 i;

  pokemon = CoursePokemonBuffer();
  friend = (PeerInfo *)IrPayload();
  friend->dailySteps = g_state.dailySteps;
  friend->hourSteps = g_state.hourSteps;
  EepromRead(
      PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, values.pokemon),
      pokemon, sizeof(Pokemon));
  friend->fixedFacing = pokemon->fixedFacing;
  friend->id = pokemon->id;
  friend->form = pokemon->form;
  friend->sex = pokemon->sex;
  friend->shiny = pokemon->shiny;
  EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP,
                   (u8 *)&g_work.irc.statusB.status, sizeof(DeviceStatus));
  friend->compatibilityLe = g_work.irc.statusB.status.consoleCompatibilityLe;
  friend->gameVersionLe = g_work.irc.statusB.status.gameVersionLe;
  /* Both record-building paths enter the copy before testing its bound. */
  i = 0;
  do {
    friend->trainerName[i] = g_work.irc.statusB.status.trainerNameData[i];
    i++;
  } while (i < TRAINER_NAME_BYTES);
  EepromRead((u16)((CourseResources *)EEPROM_COURSE)->values.nickname,
             friend->nickname, sizeof(friend->nickname));
}

#pragma inline(RequestBulkChunk)
static void RequestBulkChunk(void)
{
  u16 chunk;
  u16 src;
  u8 *out;

  const u16 remaining = g_work.irc.work.bulkBytesRemaining;
  chunk = remaining;
  if (chunk > PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
    chunk = PW_IR_BULK_MAXIMUM_CHUNK_LENGTH;
  }
  out = IrPayload();
  src = g_work.irc.work.bulkSourceEepromAddress;
  out[0] = (src >> 8);
  out[1] = src;
  out[2] = chunk;
  SendPacket(3, IR_CMD_EEPROM_READ, 2);
}

/* Receive SCI3 bytes until the inter-frame gap, check the frame checksum and
 * session token, and dispatch handshake or application commands. */
void IrProtocolTick(void)
{
  u8 size;
  u16 elapsed;
  u32 receivedToken;
  u16 received;
  u16 computed;
  IrcHeader *header;
  DeviceStatus *local;
  u8 payloadLength;
  u8 *payload;
  u8 command;
  u8 argument;

  WatchdogService();
  SCI3.SSR3.BYTE &= 0xc4;
  size = g_state.irReceiveWindow;
  if (SCI3.SSR3.BIT.RDRF != 0) {
    if (size >= PW_IR_RX_WINDOW_CAPACITY) {
      g_work.irc.work.sci3RxDrainByte = SCI3.RDR3;
      g_state.irResult = 8;
      IrFinish();
      return;
    }
    g_work.irc.packet[g_state.irReceiveWindow++] =
        (SCI3.RDR3 ^ PW_IR_TRANSPORT_XOR);
    g_state.irTimerStart = TW.TCNT;
    return;
  }

  elapsed = (TW.TCNT - g_state.irTimerStart);
  if (elapsed <= PW_IR_FRAME_GAP_TICKS) {
    return;
  }
  if (elapsed > PW_IR_INACTIVITY_TICKS) {
    g_work.irc.work.timeoutRetryCount++;
    if ((g_work.irc.work.phase >= IR_PHASE_INITIATOR) ||
        (g_work.irc.work.timeoutRetryCount >= PW_IR_TIMEOUT_RETRY_LIMIT)) {
      if (g_work.irc.work.sessionFlags.bits.receivedAny != 0) {
        g_state.irResult = 2;
      } else {
        g_state.irResult = 1;
      }
      IrFinish();
      return;
    }
    elapsed = (((RandomNext() >> 5) & PW_IR_RETRY_JITTER_MASK) *
               PW_IR_RETRY_JITTER_TICKS);
    g_state.irTimerStart = TW.TCNT;
    while ((TW.TCNT - g_state.irTimerStart) < elapsed) {
    }
    g_work.irc.work.phase = IR_PHASE_PROBING;
    IrTransmitByte(IR_CONNECT);
    elapsed = TW.TCNT;
    /* Timer W bit 14 selects the raster bank. */
    elapsed = ((elapsed >> 14) & 1);
    DisplaySelectBank(elapsed);
    g_state.irTimerStart = TW.TCNT;
    return;
  }

  if (size == 0) {
    return;
  }
  g_work.irc.work.sessionFlags.bits.receivedAny = 1;
  if (g_state.irReceiveWindow == 1) {
    /* A one-byte CONNECT probe contains only the command byte. */
    g_state.irReceiveWindow = 0;
    if (g_work.irc.packet[0] != IR_CONNECT) {
      return;
    }
    /* Answer a probe only while probing. Later handshake phases ignore
     * duplicate one-byte probes. */
    switch (g_work.irc.work.phase) {
    case IR_PHASE_PROBING:
      g_work.irc.work.phase = IR_PHASE_REPLY_SENT;
      SendPacket(0, IR_RESPONSE, 2);
      break;
    case IR_PHASE_REPLY_SENT:
      break;
    case IR_PHASE_RESPONDER:
    case IR_PHASE_INITIATOR:
      break;
    }
    return;
  }

  header = (IrcHeader *)g_work.irc.packet;
  received = (header->checksumLo + (header->checksumHi << 8));
  header->checksumLo = 0;
  header->checksumHi = 0;
  computed = PacketChecksum(g_work.irc.packet, g_state.irReceiveWindow);
  if (received != computed) {
    g_state.irReceiveWindow = 0;
    g_work.irc.work.checksumFailureCount++;
    if (g_work.irc.work.checksumFailureCount < PW_IR_CHECKSUM_FAIL_LIMIT) {
      return;
    }
    g_state.irResult = 2;
    IrFinish();
    return;
  }

  /* Preserve the received ID snapshot while deriving the session ID for each
   * handshake. */
  receivedToken = header->sessionToken;

  argument = header->argument;
  command = header->command;
  payloadLength = g_state.irReceiveWindow;
  payloadLength -= PW_IR_PACKET_HEADER_SIZE;

  payload = g_work.irc.packet + PW_IR_PACKET_HEADER_SIZE;
  if (command < IR_ACK) {
    if (receivedToken != g_work.irc.work.sessionToken) {
      goto packetDone;
    }
    if (g_work.irc.work.phase < IR_PHASE_INITIATOR) {
      goto packetDone;
    }
  }
  switch (command) {
  case IR_RESPONSE:
    if ((argument == 1) || (argument == 2)) {
      switch (g_work.irc.work.phase) {
      case IR_PHASE_PROBING:
        g_work.irc.work.phase = IR_PHASE_INITIATOR;
        SendPacket(0, IR_ACK, 2);
        g_work.irc.work.sessionToken =
            receivedToken ^ g_work.irc.work.localSessionWord;
        break;
      case IR_PHASE_RESPONDER:
      case IR_PHASE_INITIATOR:
      case IR_PHASE_REPLY_SENT:
        elapsed = (((RandomNext() >> 5) & PW_IR_RETRY_JITTER_MASK) *
                   PW_IR_RETRY_JITTER_TICKS);
        g_state.irTimerStart = TW.TCNT;
        while ((TW.TCNT - g_state.irTimerStart) < elapsed) {
        }
        g_work.irc.work.phase = IR_PHASE_PROBING;
        IrTransmitByte(IR_CONNECT);
        g_state.irTimerStart = TW.TCNT;
        break;
      }
    } else {
      g_state.irResult = 3;
      IrFinish();
    }
    break;
  case IR_SHUTDOWN:
    IrFinish();
    break;
  case IR_ACK:
    /* The acknowledgement parameter must be 2. */
    if (argument != 2) {
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (g_work.irc.work.phase < IR_PHASE_INITIATOR) {
      g_work.irc.work.sessionToken =
          receivedToken ^ g_work.irc.work.localSessionWord;
      g_work.irc.work.phase = IR_PHASE_RESPONDER;
      EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, IrPayload(),
                       sizeof(DeviceStatus));
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REQUEST, 2);
      g_ui.view.ir.role.bits.peerSender = 0;
    }
    break;
  case IR_CMD_PEER_STATUS_REQUEST:
    /* Reject invalid peer requests one check at a time, sending the
     * corresponding failure reply. */
    g_ui.view.ir.role.bits.peerSender = 1;
    local = (DeviceStatus *)IrPayload();
    g_work.irc.statusA.status = *local;
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)local,
                     sizeof(DeviceStatus));
    if (local->registered == 0) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY, 2);
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.registered == 0) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY, 2);
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.hasPokemon == 0) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY, 2);
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (local->peerProtocol != g_work.irc.statusA.status.peerProtocol) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY, 2);
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.firmwareCompatibility != 0) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY, 2);
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (local->hasPokemon == 0) {
      SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY, 2);
      g_state.irResult = 4;
      IrFinish();
      break;
    }
    if (SeenPeer(g_work.irc.statusA.status.deviceId) != 0) {
      SendPacket(0, IR_CMD_PEER_ALREADY_RECORDED, 2);
      g_state.irResult = 5;
      IrFinish();
      break;
    }
    SendPacket(sizeof(DeviceStatus), IR_CMD_PEER_STATUS_REPLY, 2);
    break;
  case IR_CMD_PEER_STATUS_REPLY:
    /* Apply the peer-request checks without sending failure replies. After all
     * checks pass, begin transfer with the Pokemon icon. */
    local = (DeviceStatus *)IrPayload();
    g_work.irc.statusA.status = *local;
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP, (u8 *)local,
                     sizeof(DeviceStatus));
    if (local->registered == 0) {
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.registered == 0) {
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.hasPokemon == 0) {
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (local->peerProtocol != g_work.irc.statusA.status.peerProtocol) {
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (g_work.irc.statusA.status.firmwareCompatibility != 0) {
      g_state.irResult = 3;
      IrFinish();
      break;
    }
    if (local->hasPokemon == 0) {
      g_state.irResult = 4;
      IrFinish();
      break;
    }
    if (SeenPeer(g_work.irc.statusA.status.deviceId) != 0) {
      SendPacket(0, IR_CMD_PEER_ALREADY_RECORDED, 2);
      g_state.irResult = 5;
      IrFinish();
      break;
    }
    BeginBulkPhase(
        PW_IRC_PEER_BULK_SEND_POKEMON_ICON,
        PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources, pokemonImage),
        EEPROM_PEER_IMAGE, sizeof(((CourseResources *)0)->pokemonImage));
    {
      u16 chunk;
      u16 dest;

      chunk = g_work.irc.work.bulkBytesRemaining;
      if (chunk > PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
        chunk = PW_IR_BULK_MAXIMUM_CHUNK_LENGTH;
      }
      EepromRead(g_work.irc.work.bulkSourceEepromAddress, IrPayload(), chunk);
      dest = g_work.irc.work.bulkDestinationEepromAddress;
      SendPacket(chunk, ((dest & 0x80) | IR_CMD_PAGE_WRITE_A), (dest >> 8));
      g_work.irc.work.bulkSourceEepromAddress =
          (g_work.irc.work.bulkSourceEepromAddress + chunk);
      g_work.irc.work.bulkDestinationEepromAddress =
          (g_work.irc.work.bulkDestinationEepromAddress + chunk);
      g_work.irc.work.bulkBytesRemaining =
          (g_work.irc.work.bulkBytesRemaining - chunk);
      g_work.irc.work.bulkChunksCompleted++;
    }
    break;
  case IR_CMD_PEER_INFO:
    if (IsPeerSender() != 0) {

      EepromWrite(PW_PEER_FRIEND_INFO_ADDRESS, IrPayload(),
                  PW_PEER_FRIEND_INFO_BYTES);
      BuildPeerInfo();
      SendPacket(sizeof(PeerInfo), IR_CMD_PEER_INFO, 2);
    } else {
      EepromWrite(PW_PEER_FRIEND_INFO_ADDRESS, IrPayload(),
                  PW_PEER_FRIEND_INFO_BYTES);
      SendPacket(0, IR_CMD_PEER_START, 2);
    }
    break;
  case IR_CMD_PEER_START:
    if (IsPeerSender() != 0) {
      SendPacket(0, IR_CMD_PEER_START, 2);
    }
    g_work.irc.work.completionAction = IR_CMD_PEER_START;
    IrFinish();
    break;
  case IR_CMD_PEER_ALREADY_RECORDED:
    g_state.irResult = 5;
    IrFinish();
    break;
  case IR_CMD_STATUS_REQUEST:
    local = (DeviceStatus *)IrPayload();
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP,
                     (u8 *)IrPayload(), sizeof(DeviceStatus));
    local->totalSteps = g_state.save.totalSteps;
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    EepromWrite(PW_EEPROM_MEMBER_ADDRESS(EEPROM_WALK, WalkData, watts),
                &g_state.save.watts, 2);
    g_work.irc.statusB.status = *local;
    SendPacket(sizeof(DeviceStatus), IR_CMD_STATUS_REPLY, 2);
    break;
  case IR_CMD_WALK_START_REQUEST:
    StatusApplyTime();
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    SendPacket(0, IR_CMD_WALK_START_ACCEPTED, 2);
    break;
  case IR_CMD_WALK_START_REJECTED:
    g_state.irResult = 3;
    IrFinish();
    break;
  case IR_CMD_WALK_START_COMMIT:
    SendPacket(0, IR_CMD_WALK_START_COMMIT, 2);
    g_work.irc.work.completionAction = IR_CMD_WALK_START_COMMIT;
    break;
  case IR_CMD_WALK_END_REQUEST:
    StatusApplyTime();
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    SendPacket(0, IR_CMD_WALK_END_ACCEPTED, 2);
    break;
  case IR_CMD_WALK_END_REJECTED:
    g_state.irResult = 3;
    IrFinish();
    break;
  case IR_CMD_WALK_END_COMMIT:
    SendPacket(0, IR_CMD_WALK_END_ACK, 2);
    g_work.irc.work.completionAction = IR_CMD_WALK_END_COMMIT;
    IrFinish();
    break;
  case IR_CMD_WALK_UPDATE_REQUEST:
    StatusApplyTime();
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    SendPacket(0, IR_CMD_WALK_UPDATE_ACCEPTED, 2);
    break;
  case IR_CMD_WALK_UPDATE_COMMIT:
    SendPacket(0, IR_CMD_WALK_UPDATE_COMMIT, 2);
    g_work.irc.work.completionAction = IR_CMD_WALK_UPDATE_COMMIT;
    IrFinish();
    break;
  case IR_CMD_WALK_UPDATE_REJECTED:
    g_state.irResult = 3;
    IrFinish();
    break;
  case IR_CMD_GIFT_REQUEST:
    StatusApplyTime();
    EepromMirrorRead(EEPROM_STATUS_PRIMARY, EEPROM_STATUS_BACKUP,
                     (u8 *)&g_work.irc.statusB.status, sizeof(DeviceStatus));
    SendPacket(0, IR_CMD_GIFT_ACCEPTED, 2);
    break;
  case IR_CMD_GIFT_COLLECTION_COMMIT:
    SendPacket(0, IR_CMD_GIFT_COLLECTION_ACK, 2);
    g_work.irc.work.completionAction = IR_CMD_GIFT_COLLECTION_COMMIT;
    break;
  case IR_CMD_GIFT_REJECTED:
    g_state.irResult = 3;
    IrFinish();
    break;
  case IR_CMD_EVENT_MAP_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= 0x10;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_MAP_DONE, 2);
    g_work.irc.work.completionAction = IR_CMD_EVENT_MAP_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_STAMP_MAP_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= 0x1f;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_MAP_DONE, 2);
    g_work.irc.work.completionAction = IR_CMD_EVENT_MAP_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_POKEMON_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= 0x20;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_POKEMON_DONE, 2);
    g_work.irc.work.completionAction = IR_CMD_EVENT_POKEMON_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_STAMP_POKEMON_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= 0x2f;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_POKEMON_DONE, 2);
    g_work.irc.work.completionAction = IR_CMD_EVENT_POKEMON_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_ITEM_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= 0x40;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_ITEM_DONE, 2);
    g_work.irc.work.completionAction = IR_CMD_EVENT_ITEM_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_STAMP_ITEM_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= 0x4f;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    SendPacket(0, IR_CMD_EVENT_ITEM_DONE, 2);
    g_work.irc.work.completionAction = IR_CMD_EVENT_ITEM_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_COURSE_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= 0x80;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    g_state.save.bonusCourse = 1;
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    SendPacket(0, IR_CMD_EVENT_COURSE_DONE, 2);
    g_work.irc.work.completionAction = IR_CMD_EVENT_COURSE_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_STAMP_COURSE_DONE: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    flags |= 0x8f;
    EepromWriteByte(EEPROM_EVENTS, flags);
    StatusSetReceived(&g_work.irc.statusB.status,
                      g_work.irc.statusA.status.receiptIndex);
    g_state.save.bonusCourse = 1;
    EepromMirrorWrite(EEPROM_SAVE_PRIMARY, EEPROM_SAVE_BACKUP,
                      (u8 *)&g_state.save, sizeof(SaveData));
    SendPacket(0, IR_CMD_EVENT_COURSE_DONE, 2);
    g_work.irc.work.completionAction = IR_CMD_EVENT_COURSE_DONE;
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_REJECTED:
    g_state.irResult = 3;
    IrFinish();
    break;
  case IR_CMD_PING_REQUEST:
    SendPacket(0, IR_CMD_PING_REPLY, 2);
    break;
  case IR_CMD_PAGE_COMPRESSED_B:
  case IR_CMD_PAGE_COMPRESSED_A: {
    u16 address;

    address = (((u16)argument << 8) + command);
    if (payloadLength == PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
      EepromWritePage(address, payload);
    } else {
      BulkDecode(payload, g_work.irc.eepromScratch);
      EepromWritePage(address, g_work.irc.eepromScratch);
    }
    SendPacket(0, IR_CMD_PAGE_REPLY, argument);
    break;
  }
  case IR_CMD_PAGE_WRITE_B:
  case IR_CMD_PAGE_WRITE_A: {
    u16 address;

    address = (((u16)argument << 8) + (command & 0x80));
    EepromWrite(address, payload, payloadLength);
    SendPacket(0, IR_CMD_PAGE_REPLY, argument);
    break;
  }
  case IR_CMD_PAGE_REPLY: {
    u16 chunk;
    u16 dest;

    if (g_work.irc.work.bulkBytesRemaining == 0) {
      switch (g_work.irc.work.bulkMode) {
      case PW_IRC_PEER_BULK_SEND_POKEMON_ICON:
        BeginBulkPhase(PW_IRC_PEER_BULK_SEND_POKE_NAME,
                       PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                                pokemonName),
                       PW_PEER_POKE_NAME_ADDRESS,
                       sizeof(((CourseResources *)0)->pokemonName));
        chunk = g_work.irc.work.bulkBytesRemaining;
        if (chunk > PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
          chunk = PW_IR_BULK_MAXIMUM_CHUNK_LENGTH;
        }
        EepromRead(g_work.irc.work.bulkSourceEepromAddress, IrPayload(), chunk);
        dest = g_work.irc.work.bulkDestinationEepromAddress;
        SendPacket(chunk, ((dest & 0x80) | IR_CMD_PAGE_WRITE_A), (dest >> 8));
        g_work.irc.work.bulkSourceEepromAddress =
            (g_work.irc.work.bulkSourceEepromAddress + chunk);
        g_work.irc.work.bulkDestinationEepromAddress =
            (g_work.irc.work.bulkDestinationEepromAddress + chunk);
        g_work.irc.work.bulkBytesRemaining =
            (g_work.irc.work.bulkBytesRemaining - chunk);
        g_work.irc.work.bulkChunksCompleted++;
        break;
      case PW_IRC_PEER_BULK_SEND_POKE_NAME:
        BeginBulkPhase(PW_IRC_PEER_BULK_SEND_RECORD, EEPROM_OWN_RECORDS,
                       EEPROM_PEER_RECORDS, sizeof(PeerRecords));
        chunk = g_work.irc.work.bulkBytesRemaining;
        if (chunk > PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
          chunk = PW_IR_BULK_MAXIMUM_CHUNK_LENGTH;
        }
        EepromRead(g_work.irc.work.bulkSourceEepromAddress, IrPayload(), chunk);
        dest = g_work.irc.work.bulkDestinationEepromAddress;
        SendPacket(chunk, ((dest & 0x80) | IR_CMD_PAGE_WRITE_A), (dest >> 8));
        g_work.irc.work.bulkSourceEepromAddress =
            (g_work.irc.work.bulkSourceEepromAddress + chunk);
        g_work.irc.work.bulkDestinationEepromAddress =
            (g_work.irc.work.bulkDestinationEepromAddress + chunk);
        g_work.irc.work.bulkBytesRemaining =
            (g_work.irc.work.bulkBytesRemaining - chunk);
        g_work.irc.work.bulkChunksCompleted++;
        break;
      case PW_IRC_PEER_BULK_SEND_RECORD:
        BeginBulkPhase(PW_IRC_PEER_BULK_RECV_POKEMON_ICON,
                       PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                                pokemonImage),
                       EEPROM_PEER_IMAGE,
                       sizeof(((CourseResources *)0)->pokemonImage));
        RequestBulkChunk();
        break;
      }
    } else {
      chunk = g_work.irc.work.bulkBytesRemaining;
      if (chunk > PW_IR_BULK_MAXIMUM_CHUNK_LENGTH) {
        chunk = PW_IR_BULK_MAXIMUM_CHUNK_LENGTH;
      }
      EepromRead(g_work.irc.work.bulkSourceEepromAddress, IrPayload(), chunk);
      dest = g_work.irc.work.bulkDestinationEepromAddress;
      SendPacket(chunk, ((dest & 0x80) | IR_CMD_PAGE_WRITE_A), (dest >> 8));
      g_work.irc.work.bulkSourceEepromAddress =
          (g_work.irc.work.bulkSourceEepromAddress + chunk);
      g_work.irc.work.bulkDestinationEepromAddress =
          (g_work.irc.work.bulkDestinationEepromAddress + chunk);
      g_work.irc.work.bulkBytesRemaining =
          (g_work.irc.work.bulkBytesRemaining - chunk);
      g_work.irc.work.bulkChunksCompleted++;
    }
    break;
  }
  case IR_CMD_EVENT_COURSE_CHECK:
  case IR_CMD_EVENT_ITEM_CHECK:
  case IR_CMD_EVENT_POKEMON_CHECK:
  case IR_CMD_EVENT_MAP_CHECK: {
    u8 i;
    u8 *out;

    StatusApplyTime();
    out = IrPayload();
    i = 0;
    do {
      *out++ = g_work.irc.statusB.status.receivedEvents[i];
      i++;
    } while (i < sizeof(g_work.irc.statusB.status.receivedEvents));
    *out = g_work.irc.statusA.status.receiptIndex;
    if (StatusHasReceived(&g_work.irc.statusB.status,
                          g_work.irc.statusA.status.receiptIndex) != 0) {
      SendPacket((sizeof(g_work.irc.statusB.status.receivedEvents) + 1),
                 IR_CMD_EVENT_ALREADY_RECEIVED, 2);
      g_state.irResult = 6;
      IrFinish();
      break;
    }
    SendPacket((sizeof(g_work.irc.statusB.status.receivedEvents) + 1), command,
               2);
    break;
  }
  case IR_CMD_EVENT_STAMP3_CHECK:
  case IR_CMD_EVENT_STAMP2_CHECK:
  case IR_CMD_EVENT_STAMP1_CHECK:
  case IR_CMD_EVENT_STAMP0_CHECK: {
    u8 i;
    u8 *out;

    StatusApplyTime();
    out = IrPayload();
    i = 0;
    do {
      *out++ = g_work.irc.statusB.status.receivedEvents[i];
      i++;
    } while (i < sizeof(g_work.irc.statusB.status.receivedEvents));
    *out = g_work.irc.statusA.status.receiptIndex;
    if (StatusHasReceived(&g_work.irc.statusB.status,
                          g_work.irc.statusA.status.receiptIndex) != 0) {
      SendPacket((sizeof(g_work.irc.statusB.status.receivedEvents) + 1),
                 IR_CMD_EVENT_ALREADY_RECEIVED, 2);
      g_state.irResult = 6;
      IrFinish();
      break;
    }
    SendPacket((sizeof(g_work.irc.statusB.status.receivedEvents) + 1), command,
               2);
    break;
  }
  case IR_CMD_EVENT_STAMP3:
  case IR_CMD_EVENT_STAMP2:
  case IR_CMD_EVENT_STAMP1:
  case IR_CMD_EVENT_STAMP0: {
    u8 flags;

    flags = EepromReadByte(EEPROM_EVENTS);
    switch (command) {
    case IR_CMD_EVENT_STAMP0:
      flags |= 1;
      g_work.irc.work.completionAction = IR_CMD_EVENT_STAMP0;
      break;
    case IR_CMD_EVENT_STAMP1:
      flags |= 2;
      g_work.irc.work.completionAction = IR_CMD_EVENT_STAMP1;
      break;
    case IR_CMD_EVENT_STAMP2:
      flags |= 4;
      g_work.irc.work.completionAction = IR_CMD_EVENT_STAMP2;
      break;
    case IR_CMD_EVENT_STAMP3:
      flags |= 8;
      g_work.irc.work.completionAction = IR_CMD_EVENT_STAMP3;
      break;
    }
    EepromWriteByte(EEPROM_EVENTS, flags);
    SendPacket(0, (command + 0x10), 2);
    IrFinish();
    break;
  }
  case IR_CMD_EVENT_ALREADY_RECEIVED:
    SendPacket(0, IR_CMD_EVENT_ALREADY_RECEIVED, 2);
    g_state.irResult = 7;
    IrFinish();
    break;
  case IR_CMD_EVENT_BUFFER_FULL:
    SendPacket(0, IR_CMD_EVENT_BUFFER_FULL, 2);
    g_state.irResult = 7;
    IrFinish();
    break;
  case IR_CMD_FACTORY_SETUP: {
    FactoryData *setup;
    u8 i;

    setup = (FactoryData *)IrPayload();
    EepromMirrorWrite(EEPROM_ID_PRIMARY, EEPROM_ID_BACKUP, setup->deviceId,
                      DEVICE_ID_BYTES);
    EepromWrite(EEPROM_COUNTERS, &setup->thresholds, sizeof(MotionThresholds));
    g_ui.view.ir.selfTestFailed = 1;
    g_work.irc.work.completionAction = IR_CMD_FACTORY_SETUP;
    switch (setup->mode) {
    case 0:
      EepromMirrorWrite(EEPROM_LCD_PRIMARY, EEPROM_LCD_BACKUP,
                        (u8 *)setup + offsetof(FactoryData, lcdParameters),
                        sizeof(setup->lcdParameters));
      break;
    case 1:
      EepromMirrorRead(EEPROM_LCD_PRIMARY, EEPROM_LCD_BACKUP,
                       g_work.irc.eepromScratch, sizeof(setup->lcdParameters));
      {
        const FactoryData *expected = setup;

        for (i = 0; i < sizeof(expected->lcdParameters); i++) {
          /* Compare the readback against the received setup record. */
          if (expected->lcdParameters[i] != g_work.irc.eepromScratch[i]) {
            g_ui.view.ir.selfTestFailed = 0;
            break;
          }
        }
      }
      break;

    case 2:
      break;
    case 3:
      EepromMirrorWrite(EEPROM_LCD_PRIMARY, EEPROM_LCD_BACKUP,
                        (u8 *)setup + offsetof(FactoryData, lcdParameters),
                        sizeof(setup->lcdParameters));
      g_work.irc.work.completionAction = IR_ACTION_FACTORY_RESET;
      break;
    }
    SendPacket(DEVICE_ID_BYTES, IR_CMD_FACTORY_SETUP, 2);
    break;
  }
  case IR_CMD_MOTION_TEST_SETUP:
    if ((argument == 1) && (payloadLength == sizeof(MotionThresholds))) {
      EepromWrite(EEPROM_COUNTERS, IrPayload(), sizeof(MotionThresholds));
      SendPacket(0, IR_CMD_MOTION_TEST_SETUP, 2);
      g_work.irc.work.completionAction = IR_CMD_MOTION_TEST_SETUP;
    }
    break;
  case IR_CMD_RESET_ALL:
    EepromMirrorRead(EEPROM_ID_PRIMARY, EEPROM_ID_BACKUP, IrPayload(),
                     DEVICE_ID_BYTES);
    SendPacket(DEVICE_ID_BYTES, IR_CMD_RESET_ALL, 2);
    g_work.irc.work.completionAction = IR_CMD_RESET_ALL;
    break;
  case IR_CMD_RESET_KEEP_STEPS:
    EepromMirrorRead(EEPROM_ID_PRIMARY, EEPROM_ID_BACKUP, IrPayload(),
                     DEVICE_ID_BYTES);
    SendPacket(DEVICE_ID_BYTES, IR_CMD_RESET_ALL, 2);
    g_work.irc.work.completionAction = IR_CMD_RESET_KEEP_STEPS;
    break;
  case IR_CMD_EEPROM_READ: {
    u16 address;
    u8 length;

    address = ((payload[0] << 8) + payload[1]);
    length = payload[2];
    EepromRead(address, IrPayload(), length);
    SendPacket(length, IR_CMD_EEPROM_REPLY, 2);
    break;
  }
  case IR_CMD_EEPROM_REPLY: {
    EepromWrite(g_work.irc.work.bulkDestinationEepromAddress, IrPayload(),
                payloadLength);

    g_work.irc.work.bulkSourceEepromAddress += payloadLength;
    g_work.irc.work.bulkDestinationEepromAddress += payloadLength;
    g_work.irc.work.bulkBytesRemaining -= payloadLength;
    g_work.irc.work.bulkChunksCompleted++;
    /* Test the state directly; request construction owns its capped count. */
    if (g_work.irc.work.bulkBytesRemaining == 0) {
      switch (g_work.irc.work.bulkMode) {
      case PW_IRC_PEER_BULK_RECV_POKEMON_ICON:
        BeginBulkPhase(PW_IRC_PEER_BULK_RECV_POKE_NAME,
                       PW_EEPROM_MEMBER_ADDRESS(EEPROM_COURSE, CourseResources,
                                                pokemonName),
                       PW_PEER_POKE_NAME_ADDRESS,
                       sizeof(((CourseResources *)0)->pokemonName));
        RequestBulkChunk();
        break;
      case PW_IRC_PEER_BULK_RECV_POKE_NAME:
        BeginBulkPhase(PW_IRC_PEER_BULK_RECV_RECORD, EEPROM_OWN_RECORDS,
                       EEPROM_PEER_RECORDS, sizeof(PeerRecords));
        RequestBulkChunk();
        break;
      case PW_IRC_PEER_BULK_RECV_RECORD:
        BuildPeerInfo();
        SendPacket(sizeof(PeerInfo), IR_CMD_PEER_INFO, 2);
        goto packetDone;
      default:
        goto packetDone;
      }
    } else {
      RequestBulkChunk();
    }
    break;
  }
  case IR_CMD_EEPROM_WRITE: {
    u16 address;

    address = (((u16)argument << 8) + payload[0]);
    EepromWrite(address, payload + 1, (payloadLength - 1));
    SendPacket(0, IR_CMD_PAGE_REPLY, argument);
    break;
  }
  case IR_CMD_RAM_WRITE: {
    u8 *dest;
    u8 i;

    dest = (u8 *)(((u16)argument << 8) + payload[0]);

    for (i = 0; i < payloadLength - 1; i++) {
      *dest++ = payload[i + 1];
    }
    SendPacket(0, IR_CMD_RAM_WRITE, argument);
    break;
  }
  default:
    break;
  }

packetDone:
  elapsed = TW.TCNT;
  /* Same Timer W bit-14 raster-bank selection as the retry path above. */
  elapsed = ((elapsed >> 14) & 1);
  DisplaySelectBank(elapsed);
  g_state.irReceiveWindow = 0;
  g_state.irTimerStart = TW.TCNT;
}

/* Stop the IR peripherals before dispatching the completed command. */
void IrFinish(void)
{
  IRR1.BIT.IRRI1 = 0;
  IO.PDR3.BYTE = 1;
  SCI3.SPCR.BYTE = 1;
  SCI3.SCR3.BYTE = 0;
  CKSTPR1.BIT.S3CKSTP = 0;
  TW.TCRW.BIT.CCLR = 1;
  TW.TMRW.BIT.CTS = 0;
  CKSTPR2.BIT.TWCKSTP = 0;
  IrComplete();
}
