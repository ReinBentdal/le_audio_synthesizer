from os import system
import argparse
import sys

def ret_chk(ret, msg):
    if (ret != 0):
        print(msg)
        exit()

def program_synth(snr, board):
    print(f"Programming of {board} synth with {snr} started")

    print('Erasing net core')
    cmd = f'nrfjprog --recover --coprocessor CP_NETWORK --snr {snr}'
    ret = system(cmd)
    ret_chk(ret, "failed to erase net core")

    print('Programming net core')
    cmd = f'nrfjprog --program bin/common_net.hex -f NRF53 -q --snr {snr} --sectorerase --coprocessor CP_NETWORK'
    ret = system(cmd)
    ret_chk(ret, "failed to program net core")

    print('Programming app core')
    if board == 'nrf5340_dk':
        cmd = f'nrfjprog --program bin/synth_dk_app.hex NRF53 -q --snr {snr} --chiperase --coprocessor CP_APPLICATION'
    elif board == 'nrf5340_audio_dk':
        cmd = f'nrfjprog --program bin/synth_audio_dk_app.hex NRF53 -q --snr {snr} --chiperase --coprocessor CP_APPLICATION'
    else:
        print('Invalid board selected')
        exit()
    ret = system(cmd)
    ret_chk(ret, "failed to program app core")

    print("Restarting device")
    cmd = f'nrfjprog -r --snr {snr}'
    ret = system(cmd)
    ret_chk(ret, "failed to restart device")

def program_headset(snr, ch):
    print(f'Programming of {ch} headset with {snr} started')

    print('Erasing net core')
    cmd = f'nrfjprog --recover --coprocessor CP_NETWORK --snr {snr}'
    ret = system(cmd)
    ret_chk(ret, "failed to erase net core")

    print('Programming net core')
    cmd = f'nrfjprog --program bin/common_net.hex -f NRF53 -q --snr {snr} --sectorerase --coprocessor CP_NETWORK'
    ret = system(cmd)
    ret_chk(ret, "failed to program net core")

    print('Programming app core')
    cmd = f'nrfjprog --program bin/headset_{ch}_app.hex NRF53 -q --snr {snr} --chiperase --coprocessor CP_APPLICATION'
    ret = system(cmd)
    ret_chk(ret, "failed to program app core")

    print("Restarting device")
    cmd = f'nrfjprog -r --snr {snr}'
    ret = system(cmd)
    ret_chk(ret, "failed to restart device")

# parse program arguments
parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=(
            "This script programs the nrf5340 as synth or headset, from precompiled bonary. The headset is supposed to be programmed on nRF5340 audio dk while the synth may be programmed on either nRF5340 dk or nRF5340 audio dk"
        ),
    )

valid_devices = ['synth', 'left', 'right']

parser.add_argument(
    '-d',
    '--device',
    type=str,
    required=True,
    choices=valid_devices,
    help='Which device to program, either the synth or headset devices',
)

valid_boards = ['nrf5340_dk', 'nrf5340_audio_dk']

parser.add_argument(
    '-b',
    '--board',
    type=str,
    required=(('-d' in sys.argv and sys.argv[sys.argv.index('-d')+1] == 'synth') or ('--device' in sys.argv and sys.argv[sys.argv.index('--device')+1] == 'synth')),
    choices=valid_boards,
    help='Only applicable of synth is selected as device. Which type of development kit to program',
)

parser.add_argument(
    '-s',
    '--snr',
    type=str,
    required=True,
    help='Serial number of device',
)

options = parser.parse_args(args=sys.argv[1:])

# start programming according to provided arguments
if options.device == 'synth':
    program_synth(options.snr, options.board)
else:
    program_headset(options.snr, options.device)

print("programmed successfully!")