from pathlib import Path

Import("env")


project_dir = Path(env.subst("$PROJECT_DIR"))
output_dir = Path(env.subst("$BUILD_DIR")) / "generated"
output_dir.mkdir(parents=True, exist_ok=True)

html = (project_dir / "web" / "control-panel.html").read_text(encoding="utf-8")
delimiter = "MOSSHTML"
if f'){delimiter}\"' in html:
    raise RuntimeError("control panel contains the raw-string delimiter")

generated = (
    "#pragma once\n"
    "#include <Arduino.h>\n\n"
    f'const char CONTROL_PANEL_HTML[] PROGMEM = R"{delimiter}({html}){delimiter}";\n'
)
header = output_dir / "control_panel_generated.h"
if not header.exists() or header.read_text(encoding="utf-8") != generated:
    header.write_text(generated, encoding="utf-8", newline="\n")

env.Append(CPPPATH=[str(output_dir)])
