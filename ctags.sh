#!/bin/bash -x
cd $(dirname $0)
ctags --langmap=tcl:+.test --exclude="*Decls.h" --fields=+a+m+n+S+t --tag-relative=yes -R {tcl,tk}
find $(pwd)/{tcl,tk} -type f -name "*.[ch]" ! -name "*Decls.h" -print0 | xargs -0 cscope -b -q -k
