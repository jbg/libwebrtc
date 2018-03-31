import PRESUBMIT
import argparse
import json
import os
import re
import subprocess
import sys

include_re = re.compile(r'^\s*#s*include\s+"([A-Za-z0-9_/-]+[.]h)"\s*$')
define_re = re.compile(r'^\s*#s*define\s+(.*)$')
open_namespace_re = re.compile(
    r'^\s*namespace\s+([A-Za-z0-9_]*)\s*([{]?)\s*$')

# Given a line iterator, return a new line iterator whose lines have
# been stripped of comments.
def SkipComments(lines):
    for line in lines:
        tokens = re.split(r'(//|")', line.rstrip())
        in_string = False
        for j in range(len(tokens)):
            if tokens[j] == "//" and not in_string:
                tokens = tokens[:j]
                break
            if tokens[j] == '"':
                in_string = not in_string
        yield "".join(tokens)

# Given the file name of a header file, return two things: the set of
# headers #included by this header; and a map whose keys are namespace
# names or "#define", and whose values are essentially a count of how
# much stuff was declared.
def ParseHeader(fn):
    includes = set()
    namespace_stats = {}  # namespace or #define -> "activity count"
    with open(fn) as f:
        curly_brace_stack = [()]
        next_ns = ()
        for line in SkipComments(f.readlines()):
            m = open_namespace_re.match(line)
            if m:
                assert line.count("}") == 0
                next_ns = (m.group(1) or "(anonymous)",)
                if m.group(2):
                    assert line.count("{") == 1
                    curly_brace_stack.append(
                        curly_brace_stack[-1] + next_ns)
                    next_ns = ()
                else:
                    assert line.count("{") == 0
                continue
            m = include_re.match(line)
            if m:
                includes.add(m.group(1))
                continue
            m = define_re.match(line)
            if m:
                macro = m.group(1).strip()
                if not macro.endswith("_H_"):
                    namespace_stats.setdefault("#define", 0)
                    namespace_stats["#define"] += 1
                # fallthrough! because multiline macros with braces...
            for c in line:
                if c == "{":
                    curly_brace_stack.append(curly_brace_stack[-1]
                                             + next_ns)
                    next_ns = ()
                elif c == "}":
                    curly_brace_stack.pop()
                elif c == ";":
                    ns = "::".join(curly_brace_stack[-1])
                    namespace_stats.setdefault(ns, 0)
                    namespace_stats[ns] += 1
    return (includes, namespace_stats)

# Is the given namespace internal?
def InternalNamespace(ns):
    return re.search(r"([A-Za-z0-9_]_|::)(impl|internal)\b", ns)

# Return the name of the API directory that the given file is located
# in, or None if it isn't in an API directory.
def InApiDir(fn):
    if fn.startswith("api/"):
        return "api"
    if fn.count("/") == 0:
        return None
    (d, f) = fn.rsplit("/", 1)
    if d in PRESUBMIT.API_DIRS:
        assert not d.startswith("api")  # handled above
        assert d
        return d
    return None

def PublicPrivateFiles(props):
    public = props["public"]
    sources = props.get("sources", [])
    if public == "*":
        return (sources, [])
    else:
        return (public, sources)

def IsPublicVisibility(visibility):
    assert isinstance(visibility, list)
    if all(v.startswith("//") for v in visibility):
        return False
    if "*" in visibility:
        return True
    raise Exception("Can't handle visibility value: %s" % visibility)

def FindVisibleHeaders(out_dir):
    gn = json.loads(subprocess.check_output(
        ["gn", "desc", "--format=json", out_dir, "*"]))
    visible_headers = set()
    for (target, props) in gn.iteritems():
        (public_files, private_files) = PublicPrivateFiles(props)
        if IsPublicVisibility(props["visibility"]):
            for fn in public_files:
                assert fn.startswith("//")
                fn = fn[2:]
                if fn.endswith(".h"):
                    visible_headers.add(fn)
    return visible_headers

def get_stats(output, out_dir):
    # Find the direct #include sets of all headers.
    headers = {}  # header name -> set of #included non-system headers
    internal_ns_headers = set()  # headers that only declare internal namespaces
    git_files = set(subprocess.check_output(["git", "ls-files"]).splitlines())
    for fn in git_files:
        if fn.endswith(".h"):
            (includes, namespace_stats) = ParseHeader(fn)
            headers[fn] = {i for i in includes if i in git_files}
            if all(InternalNamespace(ns) for ns in namespace_stats.keys()):
                internal_ns_headers.add(fn)
                output("%s ignored (%s)\n" % (fn, ", ".join(
                    namespace_stats.keys()) or "empty"))

    # Find the transitive #include sets of all headers.
    transitive_includes = {}  # header name -> set of transitive #includes
    for (h, inc) in headers.items():
        while True:
            new_inc = set(inc)
            for i in inc:
                new_inc.update(headers.get(i, []))
            if new_inc == inc:
                break
            inc = new_inc
        transitive_includes[h] = inc
    assert set(headers.keys()) == set(transitive_includes.keys())
    output("\n")

    output("Headers in API directories:\n")
    dirs = {d:len([f for f in headers.keys()
                   if InApiDir(f) == d and not f in internal_ns_headers])
            for d in PRESUBMIT.API_DIRS}
    total = sum(dirs.itervalues())
    dirs["TOTAL"] = total
    for (count, d) in sorted((v, k) for (k, v) in dirs.items()):
        output("%4d  %5.1f%%  %s\n" % (count, 100.0 * count / total, d))
    output("\n")

    output("Visible headers not in API directories:\n")
    visible_headers = {h for h in FindVisibleHeaders(out_dir)
                       if h in headers and not InApiDir(h)}
    for fn in sorted(visible_headers):
        output("  %s\n" % (fn,))
    output("Total: %d visible headers not in API directories\n"
           % (len(visible_headers),))
    output("\n")
    
    output("Non-public headers transitively #included by public headers:\n")
    files = set()
    def IsPublic(h):
        return (h in visible_headers) or InApiDir(h)
    for (header, trans_includes) in transitive_includes.items():
        if IsPublic(header):
            files.update(ti for ti in trans_includes
                         if not IsPublic(ti) and not ti in internal_ns_headers)
    for fn in sorted(files):
        output("  %s\n" % (fn,))
    output("Total: %d non-public headers #included by public headers\n"
           % (len(files),))
    output("\n")

    return (total, len(visible_headers), len(files))

def get_stats_at(commit, out_dir):
    try:
        subprocess.check_output(["git", "checkout", "-f", commit],
                                stderr=subprocess.STDOUT)
        subprocess.check_output(["gclient", "sync"],
                                stderr=subprocess.STDOUT)
        return " ".join(str(s) for s in
                        get_stats(lambda x: None, out_dir))
    except Exception, e:
        return "ERROR: %s" % (e,)

class MutFile(object):
    def __init__(self, fn):
        self.__fn = fn
        with open(self.__fn) as f:
            contents = f.read()
        self.__lines = contents.splitlines()
    def fill_in_placeholders(self, filler):
        for i in range(len(self.__lines)):
            if "[]" in self.__lines[i]:
                self.__lines[i] = self.__lines[i].replace(
                    "[]", "[" + filler(self.__lines[i]) + "]")
                self.__write_file()
    def __write_file(self):
        with open(self.__fn + ".new", "w") as f:
            for line in self.__lines:
                f.write(line + "\n")
        os.rename(self.__fn + ".new", self.__fn)
    
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('out_dir', type=str)
    parser.add_argument('--commit-list', type=str)
    args = parser.parse_args()
    if args.commit_list:
        def stats_filler(line):
            commit = line.strip().split(None, 1)[0]
            stats = get_stats_at(commit, args.out_dir)
            print "%s %s" % (commit, stats)
            sys.stdout.flush()
            return stats
        MutFile(args.commit_list).fill_in_placeholders(stats_filler)
    else:
        get_stats(sys.stdout.write, args.out_dir)

if __name__ == "__main__":
  main()
