Import("env")

def rename_firmware(source, target, env):
    import shutil
    from pathlib import Path

    build_dir = Path(env.subst("$BUILD_DIR"))
    firmware_elf = build_dir / "firmware.elf"
    firmware_bin = build_dir / "firmware.bin"

    if firmware_bin.exists():
        print(f"Firmware ready: {firmware_bin}")
        print("Copy firmware.bin to the SKR 2 microSD card, then power-cycle the board.")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", rename_firmware)