/*
 * File: fr-FR.h
 * Version: v1.0.0
 * Owner: David William Bull
 * Created: 2026-08-18
 * Last Modified: 2026-08-18
 * Description: French (fr-FR) text: the option reference and return codes, the message table, the interface fragments and the label tables.
 * Dependencies: None
 * ISA: Scalar
 * Thread-safety: N/A
 * Reviewers: David William Bull
 * License: MIT  Copyright: David William Bull
 */
#pragma once

// The one source file of PITC that is not ASCII, and so the one that can be mis-decoded: read in the system
// ANSI code page instead of UTF-8, every accented character below arrives in these literals as the two
// characters of its UTF-8 encoding, and the interface ships as mojibake that compiles cleanly. CPU.vcxproj
// pins /utf-8 in both configurations; this backstops the switch the way each ISA unit backstops its /arch,
// at the file that needs it rather than at the project setting that grants it
static_assert(sizeof(L"é") == sizeof(cwchar[2]), "fr-FR.h must be read as UTF-8: /utf-8 is absent from this build.");

// Translator's note on the layout contracts this file keeps, all of them measured rather than described:
//   The return-code column of the instructions begins at column 80, and the comment column that follows the
//   option descriptions at column 96, exactly as en-GB.h places them
//   wstrInterface[8] renders three cells of 8, 10 and 18 columns, because the rule under it in [9] and every
//   results row in [18]~[21] are drawn to those widths; [9] carries that rule and may not be re-spaced
//   [1] and [5] are the two halves of the banner and hold their fields at one width, as do [2] and [6]
//   [13]~[21] are pure format: their conversions are fixed by the lanes of the unit each one reports

inline al64 cwchptrc wstrInstructions_French =
L"\nPulsed Integrity Tests for CPUs v1.1   ---   Copyright (c) David William Bull\n"
 "\nValeurs de retour"
 "\n-----------------"
 "\n-1  : Fichier des valeurs correctes introuvable                                 0 : Test de stabilité terminé avec succès"
 "\n-2  : Nombre insuffisant de valeurs d'entrée dans le fichier"
 "\n-3  : Nombre insuffisant de valeurs de sortie dans le fichier                   1 : Valeurs correctes enregistrées dans le fichier"
 "\n-4  : Erreurs de calcul détectées lors de la génération des valeurs correctes"
 "\n-5  : Impossible de créer ou de remplacer le fichier des valeurs correctes      2 : Instructions affichées dans la console"
 "\n-6  : Échec de l'écriture de toutes les valeurs d'entrée correctes"
 "\n-7  : Échec de l'écriture de toutes les valeurs de sortie correctes"
 "\n-8  : Nom de fichier non valide pour le fichier de résultats"
 "\n-9  : Impossible de créer le fichier de résultats"
 "\n-10 : Échec de l'écriture du fichier de résultats"
 "\n-11 : Unité de traitement demandée non prise en charge par le processeur"
 "\n-12 : Plus d'une option de synchronisation des fils demandée"
 "\n-13 : Durée de test nulle ou négative demandée"
 "\n-14 : Durée d'impulsion active nulle demandée"
 "\n-15 : Aucune unité de traitement demandée"
 "\n-16 : Plus d'une unité de traitement autre que l'ALU demandée"
 "\n-17 : Impossible d'allouer la quantité de mémoire demandée"
 "\n-18 : Mémoire par fil insuffisante pour la ou les unités de traitement demandées"
 "\n-19 : Impossible de créer un fil de calcul"
 "\n-20 : Impossible d'écrire l'en-tête de \"cpu.values\""
 "\n-21 : Le contenu de \"cpu.values\" n'est pas valide pour cette compilation"
 "\n-22 : Un noyau de calcul diverge du noyau qu'il doit reproduire"
 "\n-23 : Impossible d'énumérer la topologie des processeurs du système"
 "\n-24 : Valeur absente, mal formée ou hors limites pour une option de ligne de commande"
 "\n-25 : Option de ligne de commande non reconnue"
 "\n-26 : La carte de cœurs sélectionnée ne contient aucun cœur à tester"
 "\n-27 : Niveau de cache demandé non signalé par le système\n"
 "\nOptions de ligne de commande   ---   Exemple : pitc.exe I3x Spt Tcd8.0t3600 Ua"
 "\n----------------------------"
 "\n Les options sont appliquées dans l'ordre donné : lorsque deux d'entre elles définissent la même propriété, la dernière l'emporte."
 "\n 'B' et les préréglages réinitialisent les unités de traitement et la configuration mémoire : donnez-les avant toute option 'I' ou 'M'."
 "\n Une exécution à impulsions balayées n'a pas de temps d'arrêt : ']' est donc ignoré où qu'il soit donné."
 "\n Le mode pulsé (choisi sans '[' ni ']') utilise par défaut 100 ms actives et 900 ms inactives."
 "\n Chaque test est évalué d'après le fichier \"cpu.values\" : générez-le avec 'W' avant le premier test d'une compilation.\n"
 "\n B  : Lancer le test de référence. Les options placées après 'B' remplacent les valeurs par défaut ; ex. pitc.exe B Iaf mt1024"
 "\n      Par défaut : l'ALU et la plus large unité vectorielle de chaque cœur virtuel, 8 Mo de mémoire par fil, calcul constant"
 "\n      parallèle, un délai de départ de 2000 ms et une durée de 60 secondes."
 "\n Ix : Choisir les instructions à utiliser. Indique les unités à solliciter. Les options peuvent être cumulées ; ex. I2av"
 "\n      Caches : 1==Niveau 1, 2==Niveau 2, 3==Niveau 3                                            |  Le plus haut niveau de cache donné est utilisé"
 "\n      Traitement : A==ALU, F==FPU, S==SSE2, V==AVX, X==AVX512                                   |  F, S, V et X s'excluent mutuellement"
 "\n         Au moins une unité de traitement est requise ; un niveau de cache ne nomme aucune unité et reste facultatif"
 "\n         Un niveau de cache dimensionne la mémoire par fil à sa mesure : les blocs de tous les fils sélectionnés partageant une même"
 "\n         instance de ce niveau le remplissent et, ensemble, débordent du niveau inférieur. Une option 'M' remplace les tailles"
 "\n         ainsi déduites, et un niveau que ce système ne signale pas est refusé avec -27 plutôt que testé à une autre taille"
 "\n Lx : Choisir la langue de l'interface."
 "\n      Le code est comparé sans distinction de casse à ceux que porte cette compilation : en-GB, en-US et fr-FR ; ex. Lfr-FR"
 "\n         Un code non reconnu fait l'objet d'un avertissement et laisse la langue inchangée"
 "\n Mx : Choisir la quantité de mémoire à utiliser pendant le test. Les valeurs sont en mébioctets ; ex. Mt128"
 "\n      C==Par cœur virtuel, N==Par cœur de première classe, S==Par cœur virtuel de seconde classe,"
 "\n      T==Total réparti entre tous les cœurs virtuels"
 "\n         Les deux classes de cœurs sont les cœurs sans SMT et les cœurs SMT du processeur ; sur un processeur hybride, ce"
 "\n         sont ses cœurs d'efficacité et ses cœurs de performance"
 "\n         'N' et 'S' ne couvrent qu'une classe chacun : si le processeur porte des cœurs des deux, donnez les deux, sinon la"
 "\n         classe laissée sans mémoire est refusée"
 "\n         Une option 'M' remplace les tailles qu'un 'I1', 'I2' ou 'I3' déduirait ; une taille hors de la fenêtre de résidence"
 "\n         de ce niveau fait l'objet d'un avertissement"
 "\n Ox : Options du fichier de résultats. Un nom de fichier peut être cumulé avec les autres options ; ex. O[resultats.txt]16"
 "\n      []=Nom de fichier, A=ASCII non UTF, 8=UTF-8, 16=UTF-16"
 "\n Sx : Choisir la synchronisation des cœurs. Une des trois premières options (P,R,S) peut être cumulée avec la dernière (T) ; ex. Spt"
 "\n      P==Parallèle, R==Tour de rôle, S==Échelonné, T==Synchronisé dans le temps                 |  P, R et S s'excluent mutuellement"
 "\n         'T' aligne les fronts d'impulsion de tous les fils. Sans lui, une exécution parallèle décale chaque fil d'une"
 "\n         fraction aléatoire de cycle"
 "\n Tx : Choisir les options de temps. Une des trois premières (C,F,S) peut être cumulée avec les suivantes (D,T,[,]) ; ex. Tfd1.0t12.5[100]2400"
 "\n      C==Constant, F==Impulsions de longueur fixe, S==Impulsions de longueur balayée            |  Le dernier des C, F et S donné est utilisé"
 "\n      Options globales : Dx==Délai de départ, Tx==Durée du test                                 |  'x' est une valeur décimale ; ex. d10.0"
 "\n      Impulsions fixes (en millisecondes) : [x==Durée active, ]x==Durée inactive                |  'x' est un nombre entier ; ex. [250"
 "\n      Impulsions balayées (en millisecondes) : [x==Durée du cycle                               |  Un balayage n'a pas de temps d'arrêt"
 "\n         Chaque cycle commence au repos et le rapport cyclique croît en ligne droite jusqu'à 100% à la fin de la durée du test"
 "\n Ux : Choisir l'utilisation des cœurs. Une des deux premières (C,T) peut être cumulée avec l'une des suivantes (A,E,O) ; ex. Uc!.!!...!a"
 "\n      C==Carte binaire en séquence des cœurs physiques à utiliser, T==Carte binaire en séquence des cœurs virtuels à utiliser"
 "\n         Cœur désactivé : '.' ',' '_' '-' '0'  |  Cœur activé : '!' '*' '#' '+' '1' 'x' 'X'  |  Tout autre caractère termine la carte"
 "\n         La carte est la sélection entière : un cœur qu'elle ne nomme pas n'est pas utilisé, et une sélection vide est refusée"
 "\n         'C' numérote les cœurs physiques en séquence, groupe après groupe. 'T' donne 64 caractères à chaque groupe de"
 "\n         processeurs, quel que soit le nombre de cœurs virtuels qu'il porte : les caractères au-delà du dernier cœur d'un"
 "\n         groupe sont un remplissage qu'il faut néanmoins écrire pour atteindre le groupe suivant ; la carte des fils imprime"
 "\n         chaque groupe à sa propre largeur, et non à 64"
 "\n      A==Multithreading symétrique ; force l'utilisation de tous les cœurs virtuels de chaque cœur physique actif"
 "\n      E==N'utiliser que le premier cœur virtuel de chaque cœur physique actif,"
 "\n      O==N'utiliser que le dernier cœur virtuel de chaque cœur physique actif"
 "\n         Les deux gardent un cœur virtuel par cœur physique actif, quelle que soit sa largeur SMT ; un cœur qui n'en porte"
 "\n         qu'un est gardé par l'une comme par l'autre"
 "\n W  : Écrire un nouveau fichier \"cpu.values\"."
 "\n      Le fichier est construit sous le nom \"cpu.values.tmp\" puis mis en place une fois terminé : une exécution"
 "\n      interrompue laisse tout \"cpu.values\" précédent exactement tel qu'il était."
 "\n      Le fichier n'est créé que si les résultats conservent leur intégrité sur 65 536 itérations."
 "\n      Les 512 entrées sont toutes vérifiées, et non une par fil : la vérification dure des minutes plutôt que des secondes."
 "\n      Les noyaux de calcul sont d'abord recoupés : chaque noyau mémoire et combiné avec le noyau à registres de sa propre"
 "\n      unité, et chaque noyau vectoriel avec le noyau FPU voie par voie, ce qui rend le fichier lisible sur un processeur"
 "\n      d'une autre largeur vectorielle."
 "\n -x : Préréglages de configuration. Par défaut : l'ALU et la plus large unité vectorielle, et 8 Mo de mémoire par cœur."
 "\n      1==Charge constante ; un fil par cœur physique. Durée de 10 minutes"
 "\n      2==Charge constante sur tous les cœurs virtuels. Durée de 30 minutes"
 "\n      3==Charge pulsée à tour de rôle, largeur fixe ; un fil par cœur physique. Durée de 10 minutes"
 "\n      4==Charge pulsée synchronisée, largeur fixe ; un fil par cœur physique. Durée de 10 minutes"
 "\n      5==Charge pulsée synchronisée, largeur fixe, sur tous les cœurs virtuels. Durée de 30 minutes"
 "\n      6==Charge pulsée à largeur balayée ; un fil par cœur physique. Durée de 30 minutes"
 "\n      7==Charge pulsée synchronisée à largeur balayée sur tous les cœurs virtuels. Durée de 30 minutes"
 "\n      8==Charge pulsée échelonnée, largeur fixe ; un fil par cœur physique. Durée de 1 heure"
 "\n      9==Charge pulsée synchronisée et échelonnée, largeur fixe, sur tous les cœurs virtuels. Durée de 4 heures"
 "\n      0==Charge pulsée synchronisée, largeur fixe, sur tous les cœurs virtuels, chemins ALU et SSE, 2 Mo par cœur. Durée de 1 heure\n\n";

inline cwchptrc wstrMessage_French[47] = {
   L"\nRésultats écrits avec succès dans le fichier \"%s\".\n\n",
   L"\n\nNouveau fichier \"cpu.values\" généré.\n\n",
   L"\n\nFichier \"cpu.values\" introuvable. Générez-le avec l'option 'W' en ligne de commande.\n\n",
   L"\n\nNombre insuffisant de valeurs d'entrée dans le fichier \"cpu.values\".\n\n",
   L"\n\nNombre insuffisant de valeurs de sortie dans le fichier \"cpu.values\".\n\n",
   L"\n\nErreur(s) de calcul détectée(s). Résultats non écrits.\n\n",
   L"\n\nImpossible de créer le fichier \"%s\".\n\n",
   L"\n\nÉchec de l'écriture de toutes les valeurs d'entrée dans le fichier \"cpu.values\".\n\n",
   L"\n\nÉchec de l'écriture de toutes les valeurs de sortie dans le fichier \"cpu.values\".\n\n",
   L"\nAucun nom de fichier valide pour le fichier de résultats dans l'argument \"%s\" ; 'O[nom]' est attendu.\n\n",
   L"\n\nImpossible de créer le fichier \"%s\".\n\n",
   L"\n\nÉchec de l'écriture des résultats dans le fichier \"%s\".\n\n",
   L"\nLes cœurs du processeur ne prennent pas en charge le jeu d'instructions SSE2.\n",
   L"\nLes cœurs du processeur ne prennent pas en charge le jeu d'instructions AVX.\n",
   L"\nLes cœurs du processeur ne prennent pas en charge le jeu d'instructions AVX512F.\n",
   L"\nUne seule des options 'S' P, R et S peut être active ; elles s'excluent mutuellement.\n",
   L"\nLa durée du test doit être supérieure à zéro.\n",
   L"\nLa durée d'impulsion active doit être supérieure à zéro.\n",
   L"\nAu moins une unité de traitement doit être choisie via l'option 'I' ; ex. Ia\n",
   L"\nUne seule des options 'I' F, S, V et X peut être active ; elles s'excluent mutuellement.\n",
   L"\nSeulement %lld octets de mémoire par fil ; la ou les unités de traitement demandées en exigent au moins %lld.\n",
   L"\n%lld Mo de mémoire demandés, mais seuls %lld Mo sont disponibles.\n",
   L"\nImpossible d'allouer %lld Mo de mémoire.\n",
   L"\nImpossible de créer le fil de calcul n° %d.\n\n",
   L"\nAVERTISSEMENT : impossible d'épingler le fil n° %d à un cœur ; il s'exécutera là où l'ordonnanceur le placera.\n",
   L"\n\nÉchec de l'écriture de l'en-tête du fichier \"cpu.values\".\n\n",
   L"\n\n\"cpu.values\" n'est pas un fichier de valeurs PITC. Générez-le avec l'option 'W' en ligne de commande.\n\n",
   L"\n\n\"cpu.values\" est au format version %u ; cette compilation lit la version %u. Régénérez-le avec 'W'.\n\n",
   L"\n\n\"cpu.values\" a été généré par une autre compilation, ou par d'autres noyaux de calcul. Régénérez-le avec 'W'.\n\n",
   L"\n\n\"cpu.values\" est corrompu ; son contenu ne correspond pas aux empreintes de son en-tête. Régénérez-le avec 'W'.\n\n",
   L"\n\nLe noyau %s ne concorde pas avec le noyau à registres de son unité. \"cpu.values\" non écrit.\n\n",
   L"\nImpossible d'énumérer la topologie des processeurs du système ; code d'erreur %u.\n\n",
   L"\nLe système n'a signalé aucun cœur de processeur ; il n'y a rien à tester.\n\n",
   L"\nAVERTISSEMENT : le système a %d cœurs virtuels ; cette compilation en teste %d au plus, donc %d ne seront pas testés.\n",
   L"\nProcesseur hybride : %d cœur(s) de performance en SMT à %d voie(s), et %d cœur(s) d'efficacité en SMT à %d voie(s).\n"
    "  Les deux classes de cœurs sont celles-ci plutôt que les cœurs sans SMT et les cœurs SMT, donc 'Mn' et le premier\n"
    "  relevé de cache décrivent les cœurs d'efficacité, et 'Ms' et le second décrivent les cœurs de performance.\n",
   L"\nL'option '%c' de l'argument \"%s\" exige un nombre entier de %lld à %lld.\n\n",
   L"\nL'option '%c' de l'argument \"%s\" exige une valeur décimale de %.1f à %.1f.\n\n",
   L"\nArgument de ligne de commande non reconnu : \"%s\". Lancez sans argument pour la référence des options.\n\n",
   L"\nOption '%c' non reconnue dans l'argument \"%s\". Lancez sans argument pour la référence des options.\n\n",
   L"\nAVERTISSEMENT : la langue \"%s\" n'est pas disponible dans cette compilation ; la langue de l'interface est inchangée.\n",
   L"\nLa carte de cœurs 'U' n'a sélectionné aucun cœur ; il n'y a rien à tester.\n\n",
   L"\n\nLe noyau %s ne calcule pas JobFPU élément par élément, donc un \"cpu.values\" écrit ici ne serait pas\n"
    "  lisible sur un processeur d'une autre largeur vectorielle. \"cpu.values\" non écrit.\n\n",
   L"\n\nImpossible de remplacer le fichier \"%s\" ; tout fichier précédent a été laissé exactement tel qu'il était.\n\n",
   L"\nLe système ne signale pas de cache de niveau %u ; un test ne peut donc pas y être dimensionné ici.\n\n",
   L"\nAVERTISSEMENT : les jeux de travail de niveau %u ne peuvent pas être rendus résidents à ce nombre de fils ;\n"
    "  exécution à la plus petite taille qui déborde le niveau inférieur. Au plus %u fil(s) par instance de cache de\n"
    "  niveau %u auraient pu être maintenus résidents : sélectionnez moins de cœurs pour tester le niveau lui-même.\n",
   L"\nAVERTISSEMENT : la mémoire par fil demandée est hors de la fenêtre de résidence de niveau %u, de %llu ~ %llu Kio\n"
    "  pour les cœurs de classe %u, donc cette exécution n'est pas confinée à ce niveau de cache.\n",
   L"\nAVERTISSEMENT : l'encodage ANSI du fichier \"%s\" ne peut pas représenter tous les caractères de cette langue ;\n"
    "  ces caractères ont été écrits sous forme de substituts. Demandez 'O8' ou 'O16' pour les conserver.\n"
};

inline cwchptrc wstrInterface_French[22] = {
   L"Unités :",
   L"\t Mémoire allouée : %3lld Mo\tDélai de départ : %7d ms",
   L"\t  Durée active : %d ms",
   L"\tDurée de cycle : %d ms",
   L"\nSync :  ",
   L"\t   Nombre de fils : %-3d  \tDurée maximale : %5.1f s",
   L"\tDurée inactive : %d ms",
   // The characters of [7] after its last newline are measured at run time: they are the hanging indent every
   // thread-bitmap row after the first is given, and the amount trimmed after the last, so the label may be
   // translated to any width -- but it must not end in a tab, which the measure counts as one column and a
   // console renders as several
   L"\n\nCarte des fils : ",
   L"\n  Fil   |  Unité   | Valeurs correctes",
   L"| Résultat\n--------+----------+--",
   L"\nScore de référence PITC : %lld KUPS (kibi-unités par seconde)",
   L"\nERREUR ! Cœur n°%2.1lld  Attendu : ",
   L"Obtenu :",
   // [13]~[17]: the value line Failed() prints after [11], one per processing unit, in the order of the 'unit'
   // argument that selects them: 0==AVX-512, 1==AVX, 2==SSE, 3==FPU, 4==ALU. Each carries the expected lanes,
   // then [12], then the observed lanes, and the specifier count of each is fixed by the lanes of its unit --
   // sixteen, eight, four, two and two conversions respectively, in that order and no other. The separator
   // [12] between the two halves is the only part of these five a language writes
   L"%1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f  %s %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f, %1.9f\n",
   L"%1.9f, %1.9f, %1.9f, %1.9f  %s %1.9f, %1.9f, %1.9f, %1.9f\n",
   L"%1.9f, %1.9f  %s %1.9f, %1.9f\n",
   L"%1.9f  %s %1.9f\n",
   L"%lld  %s %lld\n",
   // [18]~[21]: one results-table row per value width -- 64-bit, 128-bit, 256-bit and 512-bit. They sit here
   // because their cells have to line up under the column headers of [8] and [9], which a language owns: the
   // thread number, the ProcUnit cell and the two value cells are one layout with those headers. Every row
   // renders the ProcUnit cell 10 characters wide, and the rule below [9] is drawn to that width, so a row
   // that renders it otherwise puts the table's own separator out of true.
   // [18] serves both units of the 64-bit width and takes their name from wstrUnitsCPU; the three vector rows
   // name their unit in the literal, and this language leaves wstrUnitsCPU[2]~[4] spelled as en-GB spells them
   L"\n  #%3.1d  |  %s 64  | %16.16llX | %16.16llX | %s",
   L"\n  #%3.1d  | SSE  128 | %16.16llX%16.16llX | %16.16llX%16.16llX | %s",
   L"\n  #%3.1d  | AVX  256 | %16.16llX%16.16llX%16.16llX%16.16llX | %16.16llX%16.16llX%16.16llX%16.16llX | %s",
   // Sixteen conversions and their separators are 224 columns on one line, against the 180 GCS e2 makes a hard
   // cap; a wide literal is the one token here that cannot be broken any other way, so the row is joined by
   // concatenation exactly as the multi-line messages above are
   L"\n  #%3.1d  | AVX  512 | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX"
    " | %16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX%16.16llX | %s"
};

// The bit-indexed label tables. Both are indexed by the bit position of the property they name, so their order
// is fixed by GLOBAL_CFG's bit-fields and not by this file, and every entry must be present in every language.
// Their array type is what holds them to three characters: the banner prints each selected label in a slot of
// four columns and pads the unused slots with four spaces each, and the results table gives wstrUnitsCPU a
// fixed cell, so a wider label would silently shift every column to its right (see translations.h)
// The five processing units keep the names their instruction sets carry in every language; the cache levels
// are the three labels of this table that name something a language can say, and read here as "niveau de cache"
inline cwchar wstrUnitsCPU_French[8][4] = { L"ALU", L"FPU", L"SSE", L"AVX", L"512", L"NC1", L"NC2", L"NC3" };
inline cwchar wstrSyncCPU_French[8][4]  = { L"T-R", L"Par", L"Éch", L"S-T", L"Cst", L"I-F", L"I-B", L"Réf" };
// The verdict of one results row, indexed by Evaluate: 0==every lane matched, 1==at least one did not
inline cwchar wstrPass_French[2][8]     = { L".Passe.", L"!Échec!" };
