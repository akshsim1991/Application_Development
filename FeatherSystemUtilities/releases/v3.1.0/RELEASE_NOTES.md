# Feather System Utilities 3.1.0
- Multi-select everywhere: checkbox column, select-all header checkbox, click, Ctrl-click, Shift-click ranges, Ctrl+A, Space, arrow keys, Esc. Batch actions work on the whole selection (uninstall, enable/disable/delete/delay startup, start/stop/restart/start-type services, end tasks, clean, remove files).
- Sort by clicking any column header; filters (search, chips, drop-downs) on every list; Export… to CSV.
- New pages: Drivers (installed drivers + Windows Update driver updates), Processes, Tools (system info, restore point, SFC, DISM, chkdsk, DNS flush…). About menu with shortcuts and licences.
- Duplicates: groups are now proven byte-for-byte identical during the scan (SHA-256 first, then a full comparison), with an "identical" badge; re-checked again before deleting.
- Empty folders: a folder tree is re-verified to hold no files and is then removed deepest-first, so folders containing only empty sub-folders are now deleted.
- Open file location: runs `explorer /select` directly and verifies that an Explorer window for the folder appeared, falling back to the shell API and then to opening the folder.
- Scan folders can be added/removed from the Files page or Settings; Remove delay for delayed startup entries.
- NOT run on real Windows by the developer. Driver installs need administrator rights and internet (Windows Update).
