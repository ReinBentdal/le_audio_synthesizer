from os import system
import argparse
import sys
from colorama import Fore, Style
import subprocess
import re

def log_inf(msg):
    print(Fore.GREEN + msg + Style.RESET_ALL)

def log_err(msg):
    print(Fore.RED + msg + Style.RESET_ALL)

def ret_chk(ret, msg):
    if (ret != 0):
        log_err(msg)
        exit()

def get_connected_devices():
    stdout = subprocess.check_output('nrfjprog --ids',
                                     shell=True).decode('utf-8')
    snrs = re.findall(r'([\d]+)', stdout)
    return list(map(int, snrs))

def program_synth(snr, board):
    log_inf(f"Programming of {board} synth with {snr} started")

    log_inf('Erasing net core')
    cmd = f'nrfjprog --recover --coprocessor CP_NETWORK --snr {snr}'
    ret = system(cmd)
    ret_chk(ret, "failed to erase net core")

    log_inf('Programming net core')
    cmd = f'nrfjprog --program bin/common_net.hex -f NRF53 -q --snr {snr} --sectorerase --coprocessor CP_NETWORK'
    ret = system(cmd)
    ret_chk(ret, "failed to program net core")

    log_inf('Programming app core')
    if board == 'nrf5340_dk':
        cmd = f'nrfjprog --program bin/synth_dk_app.hex NRF53 -q --snr {snr} --chiperase --coprocessor CP_APPLICATION'
    elif board == 'nrf5340_audio_dk':
        cmd = f'nrfjprog --program bin/synth_audio_dk_app.hex NRF53 -q --snr {snr} --chiperase --coprocessor CP_APPLICATION'
    else:
        log_err('Invalid board selected')
        exit()
    ret = system(cmd)
    ret_chk(ret, "failed to program app core")

    log_inf("Restarting device")
    cmd = f'nrfjprog -r --snr {snr}'
    ret = system(cmd)
    ret_chk(ret, "failed to restart device")

def program_headset(snr, ch):
    log_inf(f'Programming of {ch} headset with {snr} started')

    log_inf('Erasing net core')
    cmd = f'nrfjprog --recover --coprocessor CP_NETWORK --snr {snr}'
    ret = system(cmd)
    ret_chk(ret, "failed to erase net core")

    log_inf('Programming net core')
    cmd = f'nrfjprog --program bin/common_net.hex -f NRF53 -q --snr {snr} --sectorerase --coprocessor CP_NETWORK'
    ret = system(cmd)
    ret_chk(ret, "failed to program net core")

    log_inf('Programming app core')
    cmd = f'nrfjprog --program bin/headset_{ch}_app.hex NRF53 -q --snr {snr} --chiperase --coprocessor CP_APPLICATION'
    ret = system(cmd)
    ret_chk(ret, "failed to program app core")

    log_inf("Restarting device")
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
    help='Serial number of device. If only one device is connected, the snr is inferred. If there are more than one device connected and --snr is not provided as an argument, you will be promted to select which one to program.',
)

arg_options = parser.parse_args(args=sys.argv[1:])

connected_devices = get_connected_devices()

# snr as provided from program arguments
if arg_options.snr != None:
    snr = arg_options.snr

# infer if only one device is connected
elif len(connected_devices) == 1:
    snr = connected_devices[0]

# promt user to select which device to program
elif len(connected_devices) > 1:
    snr_index = 0
    while(True):
        log_inf("Select device to program:")
        for i, s in enumerate(connected_devices):
            print(f'{i+1}: {s}')
        snr_index = int(input("Enter number: ")) - 1
        if snr_index < 0 or snr_index >= len(connected_devices):
            log_err("number out of range\n")
            continue
        
        break
    snr = connected_devices[snr_index]

# nothing to program
else:
    log_err("no available device to program")
    exit()

# start programming according to provided arguments
if arg_options.device == 'synth':
    program_synth(snr, arg_options.board)
else: # headset, left or right
    program_headset(snr, arg_options.device)

log_inf("Programmed successfully!")