#!/usr/bin/env python3
"""Run a silent BlueZ AVRCP target to observe a Ditoo's real media commands.

Requires python-dbus and PyGObject. Temporarily enables pairing and installs an
agent accepting only the specified device; restores adapter state on exit.
Does not launch a player, produce audio or alter the default audio output.
"""
import argparse
import json
import signal
import time

import dbus
import dbus.mainloop.glib
import dbus.service
from gi.repository import GLib, GLibUnix

PLAYER = 'org.mpris.MediaPlayer2.Player'
PROPS = 'org.freedesktop.DBus.Properties'


def event(name, **fields):
    print(json.dumps(dict(event=name, monotonic=time.monotonic(), **fields)), flush=True)


class Rejected(dbus.DBusException):
    _dbus_error_name = 'org.bluez.Error.Rejected'


class Agent(dbus.service.Object):
    def __init__(self, bus, device):
        self.path = '/ditoo_test/agent'
        super().__init__(bus, self.path)
        self.device = device

    def accept(self, device):
        if str(device) != self.device:
            raise Rejected('This test accepts only the selected Ditoo')
        event('authorized', device=str(device))

    @dbus.service.method('org.bluez.Agent1', in_signature='ou', out_signature='')
    def RequestConfirmation(self, device, passkey):
        self.accept(device)

    @dbus.service.method('org.bluez.Agent1', in_signature='o', out_signature='')
    def RequestAuthorization(self, device):
        self.accept(device)

    @dbus.service.method('org.bluez.Agent1', in_signature='os', out_signature='')
    def AuthorizeService(self, device, uuid):
        self.accept(device)

    @dbus.service.method('org.bluez.Agent1', in_signature='', out_signature='')
    def Cancel(self):
        event('pairing_cancelled')

    @dbus.service.method('org.bluez.Agent1', in_signature='', out_signature='')
    def Release(self):
        event('agent_released')


class Player(dbus.service.Object):
    def __init__(self, bus):
        self.path = '/ditoo_test/player'
        super().__init__(bus, self.path)
        self.properties = dbus.Dictionary({
            'Identity': 'Ditoo silent test target', 'PlaybackStatus': 'Playing',
            'CanPlay': True, 'CanPause': True, 'CanControl': True,
            'CanGoNext': False, 'CanGoPrevious': False, 'CanSeek': False,
            'Position': dbus.Int64(0),
            'Metadata': dbus.Dictionary({'xesam:title': 'Silent Bluetooth test',
                                        'mpris:length': dbus.Int64(60000000)}, signature='sv'),
        }, signature='sv')

    def action(self, method, state):
        self.properties['PlaybackStatus'] = state
        event('media_command', method=method, state=state)
        self.PropertiesChanged(PLAYER, {'PlaybackStatus': state}, [])

    @dbus.service.method(PLAYER, in_signature='', out_signature='')
    def Play(self):
        self.action('Play', 'Playing')

    @dbus.service.method(PLAYER, in_signature='', out_signature='')
    def Pause(self):
        self.action('Pause', 'Paused')

    @dbus.service.method(PLAYER, in_signature='', out_signature='')
    def PlayPause(self):
        state = 'Paused' if self.properties['PlaybackStatus'] == 'Playing' else 'Playing'
        self.action('PlayPause', state)

    @dbus.service.method(PLAYER, in_signature='', out_signature='')
    def Stop(self):
        self.action('Stop', 'Stopped')

    @dbus.service.method(PROPS, in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        return self.properties if interface == PLAYER else {}

    @dbus.service.method(PROPS, in_signature='ss', out_signature='v')
    def Get(self, interface, name):
        return self.GetAll(interface)[name]

    @dbus.service.signal(PROPS, signature='sa{sv}as')
    def PropertiesChanged(self, interface, changed, invalidated):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('device')
    parser.add_argument('--adapter', default='hci0')
    parser.add_argument('--seconds', type=int, default=180)
    args = parser.parse_args()
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()
    path = '/org/bluez/' + args.adapter
    device = path + '/dev_' + args.device.upper().replace(':', '_')
    adapter = bus.get_object('org.bluez', path)
    props = dbus.Interface(adapter, PROPS)
    previous = props.Get('org.bluez.Adapter1', 'Pairable')
    visible = props.Get('org.bluez.Adapter1', 'Discoverable')
    media = dbus.Interface(adapter, 'org.bluez.Media1')
    manager = dbus.Interface(bus.get_object('org.bluez', '/org/bluez'), 'org.bluez.AgentManager1')
    agent = Agent(bus, device)
    player = Player(bus)
    loop = GLib.MainLoop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        GLibUnix.signal_add(GLib.PRIORITY_DEFAULT, sig, lambda: (loop.quit(), False)[1])
    GLib.timeout_add_seconds(args.seconds, lambda: (loop.quit(), False)[1])
    registered_agent = registered_player = False
    try:
        manager.RegisterAgent(agent.path, 'NoInputNoOutput')
        registered_agent = True
        manager.RequestDefaultAgent(agent.path)
        props.Set('org.bluez.Adapter1', 'Pairable', dbus.Boolean(True))
        props.Set('org.bluez.Adapter1', 'Discoverable', dbus.Boolean(True))
        media.RegisterPlayer(player.path, player.properties)
        registered_player = True
        event('ready', adapter=str(props.Get('org.bluez.Adapter1', 'Address')), device=args.device)
        loop.run()
    finally:
        try:
            if registered_player:
                media.UnregisterPlayer(player.path)
        finally:
            try:
                if registered_agent:
                    manager.UnregisterAgent(agent.path)
            finally:
                try:
                    props.Set('org.bluez.Adapter1', 'Discoverable', visible)
                finally:
                    props.Set('org.bluez.Adapter1', 'Pairable', previous)
        event('cleanup', pairable=bool(previous), discoverable=bool(visible))


if __name__ == '__main__':
    main()
