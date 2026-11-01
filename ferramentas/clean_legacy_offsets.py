from pathlib import Path
root=Path(__file__).resolve().parent.parent/'fontes/cs2_external/src'
p=root/'offsets.hpp'; s=p.read_text(encoding='utf-8')
s='#pragma once\n#include <cstdint>\n#include "generated_offsets.hpp"\n\n'+s[s.index('namespace bones {'):]
p.write_text(s,encoding='utf-8')
p=root/'main.cpp'; s=p.read_text(encoding='utf-8')
start=s.index('static void resolve_offsets('); end=s.index('static void entity_thread_fn',start)
s=s[:start]+s[end:]
p.write_text(s,encoding='utf-8')
