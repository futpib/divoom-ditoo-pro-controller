#!/usr/bin/env python3
"""Check BLE Lua replies while a classic SDP socket remains connected.

Does not pair, play audio or replace the running app. Both connections terminate
on this computer, so this is a transport test, not proof of TV interoperability.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('device')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--binary', type=Path, default=ROOT/'target/release/divoom-ditoo-pro-controller')
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=False)
    command = [str(a.binary), '--device', a.device, '--transport', 'ble']
    env = {**os.environ, 'RUST_LOG': 'warn'}
    # Guard extension opcodes with the CLI's firmware-version check.
    subprocess.run(command+['lua', 'status'], check=True, capture_output=True,
                   timeout=45, env=env)
    request = a.output/'requests.json'
    request.write_text(json.dumps([{'command': '0x37', 'payload_hex': '7f444c554100',
                                   'response': '0x37', 'delay_ms': 1000} for _ in range(25)]))
    reply_file = a.output/'replies.jsonl'

    def count():
        return len(reply_file.read_text().splitlines())

    samples = []
    with reply_file.open('w') as out, (a.output/'cli.log').open('w') as err:
        cli = subprocess.Popen(command+['raw', 'run', str(request)], stdout=out,
                               stderr=err, env=env)
        try:
            deadline = time.monotonic()+35
            while not count() and cli.poll() is None and time.monotonic()<deadline:
                time.sleep(.1)
            assert count() and cli.poll() is None, 'BLE session did not start'
            with socket.socket(socket.AF_BLUETOOTH, socket.SOCK_SEQPACKET,
                               socket.BTPROTO_L2CAP) as s:
                s.settimeout(10)
                s.connect((a.device, 1))
                before = count()
                for i in range(5):
                    # Browse public SDP records; the first fragment is enough
                    # to prove a response. No profile connection or pairing.
                    params = bytes.fromhex('3503191002ffff35050a0000ffff00')
                    s.send(struct.pack('>BHH', 6, i+1, len(params))+params)
                    reply = s.recv(4096)
                    assert len(reply)>=8 and reply[0]==7, 'invalid SDP response'
                    assert int.from_bytes(reply[1:3], 'big')==i+1, 'SDP transaction mismatch'
                    assert int.from_bytes(reply[3:5], 'big')==len(reply)-5, 'truncated SDP response'
                    samples.append({'sdp_hex': reply.hex(), 'ble_replies': count()})
                    time.sleep(1)
                during = count()
                assert during>before, 'no BLE progress while classic socket was open'
            assert cli.wait(timeout=40)==0, 'BLE command failed'
        finally:
            if cli.poll() is None:
                cli.terminate()
                try:
                    cli.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    cli.kill()
                    cli.wait()
    replies = [json.loads(line) for line in reply_file.read_text().splitlines()]
    assert len(replies)==25
    for row in replies:
        assert row['transport']=='ble' and row['response']['ack']
        assert bytes.fromhex(row['response']['data_hex']).startswith(b'DLUA\x02')
    result = {'ble_replies': len(replies), 'ble_replies_during_classic': during-before,
              'classic_sdp_replies': len(samples), 'scope': 'same computer, two transports',
              'samples': samples}
    (a.output/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: v for k, v in result.items() if k!='samples'}))


if __name__ == '__main__':
    main()
