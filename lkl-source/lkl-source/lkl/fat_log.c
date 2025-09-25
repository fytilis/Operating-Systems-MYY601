//THEOFANIS TOMPOLIS 4855
//MARINOS ARISTIDOU 5397
//ATHANASIOS FYTILIS 5381
#include <stdio.h>
#include <string.h>
#include <lkl/lkl_fat_log.h> // Το δικό μας header

static FILE *lkl_fat_journal_file = NULL; // Χρησιμοποιούμε FILE* αντί για long fd

void lkl_fat_log_init(void) {
    if (lkl_fat_journal_file == NULL) {
        // Χρησιμοποιούμε fopen 
	// θα δημιουργήσει το αρχείο αν δεν υπάρχει, αλλά αν υπάρχει, θα προσθέτει τις νέες γραμμές στο τέλος του, χωρίς να σβήνει τα παλιά περιεχόμενα.
        lkl_fat_journal_file = fopen("journal.log", "a");
        if (lkl_fat_journal_file == NULL) {
            // Χρησιμοποιούμε perror για να δούμε το σφάλμα του συστήματος
            perror("LKL_FAT_LOG: Failed to fopen journal.log");
        } else {
             fprintf(stderr, "LKL_FAT_LOG: fopen'd journal.log successfully.\n");
        }
    }
}

void lkl_fat_log_write(const char *msg) {
    if (lkl_fat_journal_file != NULL) {
        // Χρησιμοποιούμε fprintf αντί για lkl_sys_write
        int written = fprintf(lkl_fat_journal_file, "%s", msg);
        if (written < 0) {
             perror("LKL_FAT_LOG: Error fprintf to journal.log");
        }
        // Κάνουμε flush για να είμαστε σίγουροι ότι γράφτηκε (σημαντικό!)
        fflush(lkl_fat_journal_file);
    }
}

void lkl_fat_log_close(void) {
    if (lkl_fat_journal_file != NULL) {
        FILE *file_to_close = lkl_fat_journal_file; 
        lkl_fat_journal_file = NULL;
	fflush(file_to_close); 
        int ret = fclose(file_to_close);
        if (ret != 0) {
             perror("LKL_FAT_LOG: Failed to fclose journal.log");
        } else {
             fprintf(stderr, "LKL_FAT_LOG: fclose'd journal.log successfully.\n");
        }
    }
}