# The engine test cases, one per line: name, then the command with IN/ for an input PDF (tests/engine/golden/in) and
# OUT/ for what it writes. Used by make_golden.sh (the Windows app writes the expected results) and check.sh (another
# build is compared with them). Each edit is followed by --tables on its result, so the table found afterwards is compared.
cases() {
cat <<'LIST'
tables-ruled|--tables IN/tbl_ruled.pdf OUT/r.txt
tables-full|--tables IN/tbl_full.pdf OUT/r.txt
tables-sharedpath|--tables IN/tbl_sharedpath.pdf OUT/r.txt
tables-rules|--tables IN/tbl_rules.pdf OUT/r.txt
tables-exam|--tables IN/qp.pdf OUT/r.txt
edit-same|--tableedit IN/tbl_ruled.pdf OUT/e.pdf 1 200 700
edit-cells|--tableedit IN/tbl_ruled.pdf OUT/e.pdf 1 200 700 cell 2 0 Cutting_discs insrow 4 cell 4 0 Filler_wire merge 0 1 0 2
edit-noroom|--tableedit IN/tbl_full.pdf OUT/e.pdf 1 150 760 cell 2 1 Night_shift_with_a_long_note_that_wraps_onto_more_lines_than_the_row_has_room_for
edit-shared|--tableedit IN/tbl_sharedpath.pdf OUT/e.pdf 1 150 760 cell 1 0 Washers
edit-exam|--tableedit IN/qp.pdf OUT/e.pdf 1 300 560 cell 2 1 II_/_P-cycle cell 3 1 30-10-2026
edit-delete|--tableedit IN/tbl_rules.pdf OUT/e.pdf 1 300 700 delete
hf-basic|--hfremove IN/hf_basic.pdf OUT/h.pdf OUT/r.txt both all
hf-oddeven|--hfremove IN/hf_oddeven.pdf OUT/h.pdf OUT/r.txt both all
hf-dup|--hfremove IN/hf_dup.pdf OUT/h.pdf OUT/r.txt both all
hf-section|--hfremove IN/hf_section.pdf OUT/h.pdf OUT/r.txt both all
hf-xobject|--hfremove IN/hf_xobject.pdf OUT/h.pdf OUT/r.txt both all
hf-word|--hfremove IN/hf_word.pdf OUT/h.pdf OUT/r.txt both all
hf-pages|--hfremove IN/hf_basic.pdf OUT/h.pdf OUT/r.txt both 2,4-5
LIST
}
# (Underscores in a case stand for spaces: the list is split on spaces.)
