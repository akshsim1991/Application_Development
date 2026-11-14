# VoiceMate 2.0.1 - bug fixes (found by reviewing the code; not yet confirmed on a phone)

Fixed in this version
1. Service reminders could only be added, swiped "done" or deleted. They can now be edited: tap a reminder to change
   its name, interval, and "last done at" reading.
2. Fuel fill-ups, documents (insurance / PUC / licence), saved places and ride names could not be edited. All are
   editable now (tap, or swipe right / press and hold for saved places; rides: menu at the top right).
3. Setting the odometer for the first time left every default service reminder counting from 0 km, so each showed
   "overdue by thousands of km" and was announced at the start of every ride. The first odometer entry now moves the
   reminders to that reading.
4. The odometer field only saved after tapping away, and the number pad has no Return key, so edits looked broken.
   It now opens a dialog with Save and Cancel.
5. Number-pad fields had no way to close the keyboard. A Done button now sits above the keyboard and lists can be
   dragged to dismiss it.
6. Rides could not be renamed or deleted from the ride screen. They can now.

Known limitations
- Rides shorter than 200 m are not saved.
- Garage distances are in kilometres even if Units is set to miles.
- Timers and reminders set by voice are lost if the app is closed.
