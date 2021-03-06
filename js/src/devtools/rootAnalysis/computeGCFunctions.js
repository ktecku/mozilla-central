/* -*- Mode: Javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

"use strict";

loadRelativeToScript('utility.js');
loadRelativeToScript('annotations.js');
loadRelativeToScript('loadCallgraph.js');

if (typeof scriptArgs[0] != 'string')
    throw "Usage: computeGCFunctions.js <callgraph.txt>";

print("Time: " + new Date);

loadCallgraph(scriptArgs[0]);

for (var name in gcFunctions) {
    print("");
    print("GC Function: " + name);
    do {
        name = gcFunctions[name];
        print("    " + name);
    } while (name in gcFunctions);
}

for (var name in suppressedFunctions) {
    print("");
    print("Suppressed Function: " + name);
}
