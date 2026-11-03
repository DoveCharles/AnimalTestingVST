#!/usr/bin/env python3
"""Add a donor to the list and rebuild the donor list on the support page.

    python3 donors/add_donor.py "Name from Ko-fi" 15
    python3 donors/add_donor.py "" 5            # blank name shows as [ANON]

Each donor gets a random animal from animals.txt that nobody else has. donors.json
keeps the amounts so the list can be sorted; the page itself shows only names and animals.
"""
import datetime, html, json, os, random, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ANIMALS = os.path.join(HERE, "animals.txt")
LEDGER = os.path.join(HERE, "donors.json")
PAGE = os.path.join(HERE, "..", "site", "support", "index.html")
START, END = "<!-- donors:start -->", "<!-- donors:end -->"
ANON = {"", "someone", "anon", "anonymous", "[anon]"}


def load():
    if not os.path.exists(LEDGER):
        return []
    with open(LEDGER, encoding="utf-8") as f:
        return json.load(f)


def render(donors):
    # Biggest first; equal amounts keep the order they came in
    ranked = sorted(donors, key=lambda d: (-d["amount"], d["date"]))
    if not ranked:
        return '\n<p class="donors-empty">No donors yet. Yours could be the first name here.</p>\n'
    rows = []
    for d in ranked:
        name = html.escape(d["name"]) if d["name"] else '<span class="anon">[ANON]</span>'
        rows.append(f'  <li><span class="emoji" aria-hidden="true">{d["emoji"]}</span>'
                    f'<span class="who">{name}</span><span class="animal">{html.escape(d["animal"])}</span></li>')
    return '\n<ol class="donors">\n' + "\n".join(rows) + "\n</ol>\n"


def write_page(donors):
    with open(PAGE, encoding="utf-8") as f:
        page = f.read()
    a, b = page.index(START) + len(START), page.index(END)
    with open(PAGE, "w", encoding="utf-8") as f:
        f.write(page[:a] + render(donors) + page[b:])


def main():
    if len(sys.argv) == 1:
        write_page(load())  # just rebuild the list
        return
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    name, amount = sys.argv[1].strip(), float(sys.argv[2])
    if name.lower() in ANON:
        name = ""
    donors = load()
    taken = {d["animal"] for d in donors}
    with open(ANIMALS, encoding="utf-8") as f:
        pool = [l.rstrip("\n").split("\t") for l in f if l.strip()]
    free = [p for p in pool if p[1] not in taken]
    if not free:
        sys.exit("Every animal is taken. Add more lines to animals.txt.")
    emoji, animal = random.choice(free)
    donors.append({"name": name, "amount": amount, "animal": animal, "emoji": emoji,
                   "date": datetime.datetime.now().isoformat(timespec="seconds")})
    with open(LEDGER, "w", encoding="utf-8") as f:
        json.dump(donors, f, ensure_ascii=False, indent=1)
        f.write("\n")
    write_page(donors)
    print(f"{name or '[ANON]'}: {emoji} {animal}")


if __name__ == "__main__":
    main()
