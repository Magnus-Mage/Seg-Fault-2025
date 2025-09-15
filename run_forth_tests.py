import subprocess
import os
import sys

EXAMPLES_DIR = 'examples/'

COMMAND_TEMPLATE = './build/forth_compiler {} -v --show-code --stats'
SUCCESS_PHRASE = 'Ready for Phase 5: ESP32 Integration & Optimization'

def run_test(file_path, verbose=False):
    cmd = COMMAND_TEMPLATE.format(file_path)
    try:
        result = subprocess.run(cmd, shell=True, text=True, capture_output=True)
        output = result.stdout + result.stderr

        success = SUCCESS_PHRASE in output
        status = 'PASS' if success else 'FAIL'

        print(f"{os.path.basename(file_path)}: {status}")
        if verbose:
            print(output)
            print("=" * 40)
    except Exception as e:
        print(f"{os.path.basename(file_path)}: ERROR - {e}")

def main():
    verbose = False
    if len(sys.argv) > 1 and sys.argv[1] in ('-v', '--verbose'):
        verbose = True

    files = [f for f in os.listdir(EXAMPLES_DIR) if f.endswith('.forth')]
    files.sort()
    for file_name in files:
        full_path = os.path.join(EXAMPLES_DIR, file_name)
        run_test(full_path, verbose=verbose)

if __name__ == '__main__':
    main()
