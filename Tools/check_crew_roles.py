# Proves the chain of the hired people on the source (to-do 300; UStaffPeople::CheckRoles does the same at the start): every duty
# belongs to a role that can be hired and comes in 3D, every task has its people, all of one role, every vehicle asks for the role
# of its own team, every role has a job or says that it has none, the numbers needed are never under the smallest team.
import os, re, sys
S = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Source', 'AirportDynastyPro')
rd = lambda p: open(os.path.join(S, p), encoding='utf-8').read()
head, sp, crew, svc, gr = rd('Public/StaffPeople.h'), rd('Private/StaffPeople.cpp'), rd('Private/Sim/ADPCrew.cpp'), rd('Private/GSEService.cpp'), rd('Private/Sim/ADPGround.cpp')
bad = []
enum = lambda n: [x.strip() for x in re.sub(r'//[^\n]*', '', re.search(r'enum class %s[^{]*\{([^}]*)\}' % n, head).group(1)).split(',') if x.strip()]
duties, tasks = enum('EStaffDuty'), enum('EStaffTask')
for n, last in (('NumDuties', duties[-1]), ('NumTasks', tasks[-1])):
    if not re.search(n + r' = int32\(EStaff\w+::%s\) \+ 1' % last, head): bad.append('%s is not up to %s' % (n, last))
roles = {}
for m in re.finditer(r'\{TEXT\("(\w+)"\), TEXT\("([^"]+)"\), TEXT\("(\w+)"\), (true|false), (true|false), (true|false), (true|false), (nullptr|TEXT\("[^"]+"\))\}', crew):
    roles[m[1]] = dict(d3=m[4] == 'true', sim=m[5] == 'true', shift=m[6] == 'true', work=None if m[8] == 'nullptr' else m[8][6:-2])
if len(roles) < 20: bad.append('only %d roles read from ADPCrew.cpp' % len(roles))
ro = re.search(r'UStaffPeople::RoleOf\(EStaffDuty D\) \{(.*?); \}', sp, re.S).group(1)
role, rest = {}, ro
for m in re.finditer(r'((?:D == EStaffDuty::\w+(?: \|\| )?)+) \? TEXT\("(\w+)"\)', ro):
    for d in re.findall(r'EStaffDuty::(\w+)', m[1]): role[d] = m[2]
last = re.findall(r'TEXT\("(\w+)"\)', ro)[-1]
for d in duties: role.setdefault(d, last)
i = sp.index('UStaffPeople::Needs(')
needs = {}
for m in re.finditer(r'((?:case EStaffTask::\w+:\s*)+)(.*?)(?:break;|return;)', sp[i:i + 5000], re.S):
    for t in re.findall(r'EStaffTask::(\w+)', m[1]): needs.setdefault(t, [(a, int(b)) for a, b in re.findall(r'Add\(EStaffDuty::(\w+), (\d+)', m[2])])
used = set(role.values())
for d in duties:
    r = roles.get(role[d])
    if not r: bad.append('duty %s: no role %s' % (d, role[d]))
    elif not (r['d3'] and r['shift']): bad.append('duty %s: role %s never comes' % (d, role[d]))
    if not any(d == a for v in needs.values() for a, _ in v): bad.append('duty %s: no task needs it' % d)
for t in tasks:
    n = needs.get(t, [])
    if not n or sum(b for _, b in n) < 1: bad.append('task %s: no duties' % t)
    elif len({role[a] for a, _ in n}) > 1: bad.append('task %s: mixes roles %s' % (t, {role[a] for a, _ in n}))
pairs = re.findall(r'\.Role = TEXT\("(\w+)"\); \w+\.Team = EStaffTask::(\w+)', svc)
for r, t in pairs:
    if needs.get(t) and role[needs[t][0][0]] != r: bad.append('vehicle asks for %s, its team %s is %s' % (r, t, role[needs[t][0][0]]))
for k, r in roles.items():
    if r['d3'] and k not in used: bad.append('role %s: comes in 3D, no duty' % k)
    if r['work'] is None and (r['d3'] or r['sim'] or k in used): bad.append('role %s: has a job, its line says none' % k)
    if r['work'] and not (r['d3'] or r['sim']): bad.append('role %s: its line says a job, nothing uses it' % k)
least = {k: max([1] + [sum(b for a, b in v if role[a] == k) for v in needs.values()]) for k in used}
tn = {a: int(b) for a, b in re.findall(r'\{TEXT\("(\w+)"\), (\d+)\}', re.search(r'TeamNeeds\(.*?\{ Out = (.*)', gr).group(1))}
for k, v in least.items():
    if tn.get(k, 0) < v: bad.append('role %s: the list needs %d, the smallest team is %d' % (k, tn.get(k, 0), v))
print('%d duties, %d tasks, %d roles, %d vehicles; smallest teams %s' % (len(duties), len(tasks), len(roles), len(pairs), least))
print('no job in this version: ' + ', '.join(k for k, r in roles.items() if not r['work']))
for b in bad: print('FAULT', b)
print('%d faults' % len(bad))
sys.exit(1 if bad else 0)
