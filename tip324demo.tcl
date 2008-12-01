proc tip324demo {} {
    wm title . "TIP324 Demo"
    tk fontchooser configure -parent .
    button .b -command fcToggle -takefocus 0; fcVisibility .b
    bind . <<TkFontchooserVisibility>> [list fcVisibility .b]
    foreach w {.t1 .t2} {
        text $w -width 20 -height 4 -borderwidth 1 -relief solid
        bind $w <FocusIn> [list fcFocus $w]
        $w insert end "Text Widget $w"
    }
    .t1 configure -font {Courier 14}
    .t2 configure -font {Times 16}
    pack .b .t1 .t2; focus .t1
}
proc fcToggle {} {
    tk fontchooser [expr {[tk fontchooser configure -visible] ?
            "hide" : "show"}]
}
proc fcVisibility {w} {
    $w configure -text [expr {[tk fontchooser configure -visible] ?
            "Hide Font Dialog" : "Show Font Dialog"}]
}
proc fcFocus {w} {
    tk fontchooser configure -font [$w cget -font] -command [list fcFont $w]
}
proc fcFont {w font args} {
    $w configure -font [font actual $font]
}
tip324demo
