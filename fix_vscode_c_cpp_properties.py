from pathlib import Path

try:
    Import("env")  # type: ignore  # PlatformIO SCons function
except Exception:
    env = None  # type: ignore


TARGET = '"C:/Users/Dev_Team2/.platformio/packages/framework-arduinoespressif32-libs/esp32p4_es/include/esp_system/port/soc",'


def fix_c_cpp_properties() -> None:
    if env is not None:
        project_dir = Path(env.get("PROJECT_DIR"))
    else:
        project_dir = Path.cwd()
    c_cpp_properties = project_dir / ".vscode" / "c_cpp_properties.json"

    if not c_cpp_properties.exists():
        print("c_cpp_properties.json not found, skipping IntelliSense path cleanup")
        return

    original = c_cpp_properties.read_text(encoding="utf-8")
    lines = original.splitlines(keepends=True)
    filtered = [line for line in lines if TARGET not in line]
    removed = len(lines) - len(filtered)

    if removed == 0:
        print("No stale IntelliSense include paths found")
        return

    c_cpp_properties.write_text("".join(filtered), encoding="utf-8")
    print(f"Removed {removed} stale IntelliSense include path entries from {c_cpp_properties}")


fix_c_cpp_properties()
