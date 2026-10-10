#pragma once

namespace lva::tr {

// Control channel to sendspin-client over a Unix domain datagram socket.
//
// lva owns the tap key and forwards tap-derived commands here. sendspin-client
// binds the socket and acts on the datagrams in its main loop. Fire-and-forget
// and best-effort: a datagram sent while sendspin-client is down is dropped.
//
// Socket path: /tmp/sendspin-control.sock

// Toggle music play/pause.
void SendspinMusicToggle();

// Toggle the sendspin LED effect.
void SendspinLedToggle();

// Duck / unduck sendspin playback volume during voice interaction.
void SendspinDuck();
void SendspinUnduck();

}  // namespace lva::tr
