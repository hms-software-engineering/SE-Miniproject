#!/usr/bin/env python3
"""Fill Sprint, Story Points and Priority on items ALREADY in the project (no new issues).
Run from the same folder as hms_backlog.csv. Set OWNER and PROJECT_NUMBER like in import_backlog.py.
"""
import csv, json, subprocess, sys
OWNER = "hms-software-engineering"
PROJECT_NUMBER = 3   # CHANGE-ME

def gh(*args, inp=None):
    r = subprocess.run(["gh", *args], input=inp, capture_output=True, text=True)
    if r.returncode != 0: raise RuntimeError(r.stderr.strip() or r.stdout.strip())
    return r.stdout.strip()
def gql(q, v):
    return json.loads(gh("api", "graphql", "--input", "-", inp=json.dumps({"query": q, "variables": v})))["data"]

pid = json.loads(gh("project", "view", str(PROJECT_NUMBER), "--owner", OWNER, "--format", "json"))["id"]
nodes = gql("""query($id:ID!){node(id:$id){... on ProjectV2{fields(first:50){nodes{
  ... on ProjectV2Field{id name}
  ... on ProjectV2IterationField{id name configuration{iterations{id title startDate}}}
  ... on ProjectV2SingleSelectField{id name options{id name}}}}}}}""", {"id": pid})["node"]["fields"]["nodes"]
F = {f["name"]: f for f in nodes if f}
print("Fields found:", ", ".join(F))
for need in ("Sprint", "Story Points", "Priority"):
    if need not in F: print(f"WARNING: no field named exactly '{need}' - it will be skipped")
if "Priority" in F and "options" in F["Priority"]:
    print("Priority options:", [o["name"] for o in F["Priority"]["options"]])
iters = sorted(F["Sprint"]["configuration"]["iterations"], key=lambda i: i["startDate"]) if "Sprint" in F else []
M = """mutation($p:ID!,$i:ID!,$f:ID!,$v:ProjectV2FieldValue!){
  updateProjectV2ItemFieldValue(input:{projectId:$p,itemId:$i,fieldId:$f,value:$v}){projectV2Item{id}}}"""
def setv(item, name, value): gql(M, {"p": pid, "i": item, "f": F[name]["id"], "v": value})

items = json.loads(gh("project", "item-list", str(PROJECT_NUMBER), "--owner", OWNER, "--limit", "200", "--format", "json"))["items"]
by_id = {i["title"].split()[0]: i["id"] for i in items if i.get("title")}
for n, r in enumerate(csv.DictReader(open("hms_backlog.csv", encoding="utf-8")), 1):
    item = by_id.get(r["id"])
    if not item: print("  not found in project:", r["id"]); continue
    if "Sprint" in F and len(iters) >= 2: setv(item, "Sprint", {"iterationId": iters[int(r["sprint"]) - 1]["id"]})
    if "Story Points" in F: setv(item, "Story Points", {"number": float(r["story_points"])})
    if "Priority" in F:
        if "options" in F["Priority"]:
            opt = [o["id"] for o in F["Priority"]["options"] if o["name"].lower() == r["priority"].lower()]
            if opt: setv(item, "Priority", {"singleSelectOptionId": opt[0]})
            else: print("  no matching Priority option for", r["id"], r["priority"])
        else:
            setv(item, "Priority", {"text": r["priority"]})
    print(f"[{n}] {r['id']} updated")
print("Done.")