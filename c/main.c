#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define noire 128
#define ON  0x90
#define OFF 0x80
#define C3  60
#define percu 9
#define reverb 0x5B
#define chorus 0x5D
#define phaser 0x5F

// #define MAX_TIME 7864319
#define MAX_TIME 7864319

char total_max_length = 0;

int g(int time,int bitmask,int note_index,int octave_shift_down){
    // Assumes bitmask is 0..3
    // Assumes note_index is 0..8
    char* chord;

    // Extracts bits index 16,17,18 of time
    int time_middle_bits = 3&time>>16;

    // Decides chord based on that
    if (time_middle_bits != 0) {
        chord = "BY}6YB6%";
    } else {
        chord = "Qj}6jQ6%";
    }

    // Picks a chord based on the note_index. Unsafe code.
    int picked_note = chord[note_index];

    // The picked note is turned into a base frequency (which translates into xxx Hz) by adding
    int frequency = picked_note + 51;

    // Picks a sample by multiplying time by frequency and pitch shifting it octave_shit_down number of octaves down
    int sample = (time*frequency)>>octave_shift_down;

    // Picks the first two bits of the sample, masks it with the bitmask (possibly to set the volume), then multiplies the sample height by 16 (2**4 or 1 << 4)
    int amplified_sample = (sample & bitmask & 3) << 4;
    
    return amplified_sample;
};

void MIDI_ecrire_en_tete(FILE *fichier, unsigned char SMF, unsigned short pistes, unsigned short nbdiv) {  
    if ((SMF == 0) && (pistes > 1)) {
        printf("ERREUR : une seule piste possible en SMF 0 ! \n") ;

        exit(1);
    }

    unsigned char octets[14] = {0x4d, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06} ;
    octets[8]  = 0 ;
    octets[9]  = SMF ;
    octets[10] = pistes >> 8 ;
    octets[11] = pistes ;  
    octets[12] = nbdiv  >> 8 ;
    octets[13] = nbdiv ;     // Nombre de divisions de la noire
    fwrite(&octets, 14, 1, fichier) ;
}

unsigned long MIDI_ecrire_en_tete_piste(FILE *fichier) {
    unsigned char octets[8] = {0x4d, 0x54, 0x72, 0x6b, 0x00, 0x00, 0x00, 0x00};
    fwrite(&octets, 8, 1, fichier);
    return ftell(fichier);
}


void ecrire_variable_length_quantity(FILE *fichier, unsigned long i) {
    int continuer;

    if (i > 0x0FFFFFFF) {
        printf("ERREUR : delai > 0x0FFFFFFF ! \n") ;
        exit(1) ;
    }

    unsigned long filo = i & 0x7F ;
    i = i >> 7 ;

    while (i != 0) {
        filo = (filo << 8)  + ((i & 0x7F) | 0x80) ;
        i = i >> 7 ;
    }

    do {
        fwrite(&filo, 1, 1, fichier) ;
        continuer = filo & 0x80 ;
        if (continuer) filo = filo >> 8 ;
    } while (continuer) ;    
}

void MIDI_delta_time(FILE *fichier, unsigned long duree) {
    ecrire_variable_length_quantity(fichier, duree) ;
}

void MIDI_fin_de_la_piste(FILE *fichier) {
    MIDI_delta_time(fichier, 0);
    unsigned char octets[3] = {0xFF, 0x2F, 0x00};
    fwrite(&octets, 3, 1, fichier);
}

void ecrire_taille_finale_piste(FILE *fichier, unsigned long marque) {
    unsigned char octets[4];
    unsigned long taille = ftell(fichier) - marque;
    fseek(fichier, marque-4, SEEK_SET);
    octets[0] = taille >> 24;
    octets[1] = taille >> 16;
    octets[2] = taille >> 8;
    octets[3] = taille;
    fwrite(&octets, 4, 1, fichier);
    fseek(fichier, 0, SEEK_END);
}

void MIDI_Program_Change(FILE *fichier, unsigned char canal, unsigned char instrument) {
    unsigned char octets[2];
    MIDI_delta_time(fichier, 0);
    octets[0] = 0xC0 + canal % 16;
    octets[1] = instrument % 128;
    fwrite(&octets, 2, 1, fichier);
}

void MIDI_tempo(FILE *fichier, unsigned long duree) {
    MIDI_delta_time(fichier, 0);
    unsigned char octets[6] = {0xFF, 0x51, 0x03};
    octets[3] = duree >> 16;
    octets[4] = duree >> 8;
    octets[5] = duree;
    fwrite(&octets, 6, 1, fichier);
}

void MIDI_Note(unsigned char etat, FILE *fichier, unsigned char canal, unsigned char Note_MIDI, unsigned char velocite) {
    unsigned char octets[3];
    octets[0] = etat + canal % 16;
    octets[1] = Note_MIDI % 128;
    octets[2] = velocite % 128;
    fwrite(&octets, 3, 1, fichier);
}

void Note_unique_avec_duree(FILE *fichier, unsigned char canal, unsigned char Note_MIDI, unsigned char velocite, unsigned long duree) {
    MIDI_delta_time(fichier, 0);
    MIDI_Note(ON, fichier, canal, Note_MIDI, velocite);
    MIDI_delta_time(fichier, duree);
    MIDI_Note(OFF, fichier, canal, Note_MIDI, 0);
}

int getmidifromfreq(float freq) {
    return (int)(log(freq / 440.0)/log(2) * 12 + 69);
}

void ecrire_metadata_piste(FILE *fichier) {
    unsigned long marque = MIDI_ecrire_en_tete_piste(fichier);
    int tempo = 16384 * (1000 * 1000 / 8000);
    MIDI_tempo(fichier, tempo);
    MIDI_fin_de_la_piste(fichier);
    ecrire_taille_finale_piste(fichier, marque);
}

void ecrire_piste(FILE *fichier, char *track, int note_length, int track_number) {   
    unsigned long marque = MIDI_ecrire_en_tete_piste(fichier);
    int duree = noire * note_length / 16384;
    // int duree = noire;
    // always piano
    MIDI_Program_Change(fichier, track_number, 1);

    for (int i = 0; i < MAX_TIME / note_length; i++) {
        int nbr_not_zero = 0;
        int was_zero = 1;
        char max_length = 0;
        for (int j = 0; j < note_length; j++) {
            char length = track[i * note_length + j];
            if (length > max_length) {
                max_length = length;
            }
            if (length == 0) {
                was_zero = 1;
            } else {
                if (was_zero) {
                    nbr_not_zero++;
                    was_zero = 0;
                }
            }
        }

        if (max_length > total_max_length) {
            total_max_length = max_length;
        }

        if (nbr_not_zero > 1) {
            float freq = nbr_not_zero / (note_length / 8000.0);
            int note = getmidifromfreq(freq);
            Note_unique_avec_duree(fichier, track_number, note, 64 + max_length, duree);
        } else {
            Note_unique_avec_duree(fichier, track_number, C3, 0, duree);
        }
    }

    MIDI_fin_de_la_piste(fichier);
    ecrire_taille_finale_piste(fichier, marque);
}

char track[4][MAX_TIME];

void main(void){
    int time;
    int note_length[4] = {16384, 8192, 2048, 1024};
    int note_beats[4];

    for (int i = 0; i < 4; i++) {
        note_beats[i] = note_length[i] / 1024;
    }

    for(time=0;time < MAX_TIME;time++) {
        int n = time>>14;
        int s = time>>17;
        track[0][time] = g(time, 1, n & 7, 12);
        track[1][time] = g(time, s & 3, (n ^ time >> 13) & 7, 10);
        track[2][time] = g(time, s / 3 & 3, (n + ((time >> 11) % 3)) & 7, 10);
        track[3][time] = g(time, s / 5 & 3, (8 + n - ((time >> 10) % 3)) & 7, 9);
    }

    FILE *fichier_midi = fopen("song.mid", "wb") ;
    MIDI_ecrire_en_tete(fichier_midi, 1, 5, noire);
    ecrire_metadata_piste(fichier_midi);
    total_max_length = 0;
    ecrire_piste(fichier_midi, track[0], note_length[0], 0);
    printf("Max length: %d\n", total_max_length);
    total_max_length = 0;
    ecrire_piste(fichier_midi, track[1], note_length[1], 1);
    printf("Max length: %d\n", total_max_length);
    total_max_length = 0;
    ecrire_piste(fichier_midi, track[2], note_length[2], 2);
    printf("Max length: %d\n", total_max_length);
    total_max_length = 0;
    ecrire_piste(fichier_midi, track[3], note_length[3], 3);
    printf("Max length: %d\n", total_max_length);
    fclose(fichier_midi);
}
