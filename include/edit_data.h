#ifndef EDIT_DATA_H
#define EDIT_DATA_H

#ifndef DATEN_VERZEICHNIS
#ifdef DATA_DIRECTORY
#define DATEN_VERZEICHNIS DATA_DIRECTORY
#else
#define DATEN_VERZEICHNIS "data"
#endif
#endif

#ifndef DATA_DIRECTORY
#define DATA_DIRECTORY DATEN_VERZEICHNIS
#endif

void bearbeite_daten_menue(const char *verzeichnis);

#endif
