from pathlib import Path
import hashlib, json, os
ROOT = Path(__file__).resolve().parent
for item in json.loads((ROOT / "partes/manifest.json").read_text(encoding="utf-8")):
    target = ROOT / item["path"]
    if target.exists():
        print("Ja existe:", item["path"])
        continue
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(target.name + ".restaurando")
    digest = hashlib.sha256()
    size = 0
    try:
        with temporary.open("xb") as output:
            for part in item["parts"]:
                with (ROOT / part).open("rb") as source:
                    while data := source.read(1024 * 1024):
                        output.write(data)
                        digest.update(data)
                        size += len(data)
        if size != item["size"] or digest.hexdigest() != item["sha256"]:
            raise ValueError("Falha de integridade: " + item["path"])
        os.replace(temporary, target)
        print("Restaurado:", item["path"])
    except Exception:
        temporary.unlink(missing_ok=True)
        raise
