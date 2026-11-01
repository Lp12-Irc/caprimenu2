from pathlib import Path
p=Path(__file__).parent.parent/'fontes/cs2_external/src/offsets.hpp'
s=p.read_text(encoding='utf-8'); start=s.index('namespace bones {'); end=s.index('namespace weapons {',start)
s=s[:start]+'#include "skeleton.hpp"\n\n'+s[end:]
p.write_text(s,encoding='utf-8')
