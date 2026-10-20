#!/usr/bin/env python3
"""Create GitHub issues from hms_backlog.csv, add them to your GitHub Project and set
Sprint, Story Points, Priority and Assignee.

Setup (once):
  1. Install GitHub CLI: https://cli.github.com
  2. gh auth login
  3. gh auth refresh -s project
Edit OWNER, REPO and PROJECT_NUMBER below (the number is in the project URL: .../projects/<number>).
Test first:  python import_backlog.py --dry-run
Run:         python import_backlog.py
Project needs fields named exactly: Sprint (Iteration, 2 iterations), Story Points (Number)
and optionally Priority (Single select with High / Medium / Low).
"""
import csv, json, subprocess, sys
OWNER = "hms-software-engineering"
REPO = "SE-Miniproject"          # repository name
PROJECT_NUMBER = 3          # CHANGE-ME
DRY = "--dry-run" in sys.argv

def gh(*args, inp=None):
    r = subprocess.run(["gh", *args], input=inp, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.strip() or r.stdout.strip())
    return r.stdout.strip()

def gql(query, variables):
    out = gh("api", "graphql", "--input", "-", inp=json.dumps({"query": query, "variables": variables}))
    return json.loads(out)["data"]

rows = list(csv.DictReader(open("hms_backlog.csv", encoding="utf-8")))
print(f"{len(rows)} backlog items")
if DRY:
    for r in rows: print(f"  S{r['sprint']} {r['story_points']:>2}  {r['assignee']:<15} {r['title']}")
    sys.exit(0)

pid = json.loads(gh("project", "view", str(PROJECT_NUMBER), "--owner", OWNER, "--format", "json"))["id"]
fields = gql("""query($id:ID!){node(id:$id){... on ProjectV2{fields(first:50){nodes{
  ... on ProjectV2Field{id name}
  ... on ProjectV2IterationField{id name configuration{iterations{id title startDate}}}
  ... on ProjectV2SingleSelectField{id name options{id name}}}}}}}""", {"id": pid})["node"]["fields"]["nodes"]
F = {f["name"]: f for f in fields if f}
for need in ("Sprint", "Story Points"):
    if need not in F: sys.exit(f"Project field '{need}' not found. Create it first.")
iters = sorted(F["Sprint"]["configuration"]["iterations"], key=lambda i: i["startDate"])
if len(iters) < 2: sys.exit("Sprint field needs two iterations (19 Oct and 26 Oct).")
M = """mutation($p:ID!,$i:ID!,$f:ID!,$v:ProjectV2FieldValue!){
  updateProjectV2ItemFieldValue(input:{projectId:$p,itemId:$i,fieldId:$f,value:$v}){projectV2Item{id}}}"""
def setv(item, name, value):
    gql(M, {"p": pid, "i": item, "f": F[name]["id"], "v": value})

for n, r in enumerate(rows, 1):
    base = ["issue", "create", "-R", f"{OWNER}/{REPO}", "--title", r["title"], "--body", r["body"]]
    try:
        url = gh(*base, "--assignee", r["assignee"]).splitlines()[-1]
    except RuntimeError as e:
        print(f"  ! assignee {r['assignee']} not accepted ({e}); creating unassigned")
        url = gh(*base).splitlines()[-1]
    item = json.loads(gh("project", "item-add", str(PROJECT_NUMBER), "--owner", OWNER, "--url", url, "--format", "json"))["id"]
    setv(item, "Sprint", {"iterationId": iters[int(r["sprint"]) - 1]["id"]})
    setv(item, "Story Points", {"number": float(r["story_points"])})
    if "Priority" in F:
        if "options" in F["Priority"]:      # Single select field
            opt = [o["id"] for o in F["Priority"]["options"] if o["name"].lower() == r["priority"].lower()]
            if opt: setv(item, "Priority", {"singleSelectOptionId": opt[0]})
        else:                               # Text field
            setv(item, "Priority", {"text": r["priority"]})
    print(f"[{n}/{len(rows)}] {r['title']}")
print("Done.")