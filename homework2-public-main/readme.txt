            PCom - Tema 2 (2023-2024) - Aplicatie client-server TCP si UDP pentru gestionarea mesajelor
            Iuga Darius-Gabriel-Ioan

Structuri de date folosite:
    - client: contine socketul, idul, ipul, portul si un flag de stare (offline/online) pentru un client TCP
    - subscriber: contine un pointer catre un client TCP activ
    - UDP_packet: incapsuleaza datele dintr-un packet primit de la un client UDP: idul si portul clientului; numele
topicului, tipul de date si continutul mesajului, si mesajul final formatat.
    - topic: contine numele unui topic si un vector de subscriberi (clienti TCP)

Subscriber:
    Mai intai creez un socket TCP, dezactivez algoritmul lui Nagle pentru o latenta mai mica si conectez socketul TCP
la server prin IP-ul serverului. Pentru multiplexarea intre stdin si socketul conectat la server am folosit select
deoarece are un API destul de simplu si o performanta buna pentru un numar mic de file descriptori.
    Cand primesc date de la stdin, trimit bufferul cu mesajul catre server pentru procesare si executie a comenzii.
Daca utilizatorul scrie "exit" sau "\n", voi inchide socketul si termin programul.
    Cand primesc date de la server, le afisez la stdout. Daca serverul trimite mesajul "exit" sau numarul de bytes
primiti este 0 (serverul e offline), termin executia clientului.

Server:
    Am folosit select pentru multiplexare din aceleasi motive. In setul de read file descriptors am adaugat stdin,
socketul UDP, socketul TCP care asculta, si voi adauga socketul TCP al unui client atunci cand acesta se conecteaza,
    Astept dupa 4 tipuri de evenimente: o comanda primita la stdin, un mesaj pe socketul UDP de la un client, o cerere
de conectare a unui client pe socketul TCP care asculta, sau o comanda de la un client TCP pe socketul lui.

Input pe stdin:
    Daca mesajul este "exit" sau "\n" (utilizatorul introduce enter), apelez functia close_clients si opresc serverul.

Primirea unui mesaj de la un client UDP:
    Daca numarul de bytes primiti este 0 inseamna ca acest client s-a inchis. Altfel, voi apela functia get_udp_packet
pentru a parsa datele din buffer. Daca strucura datelor a fost corecta, voi scrie mesajul formatat. Acest mesaj este trimis catre toti clientii care sunt
abonati la topicul respectiv prin send_udp_packet.

Trimiterea unui mesaj UDP:
    Pentru a gasi indexul unui topic care se potriveste, voi apela get_topic_index, care face match pe wildcarduri din
vectorul topics. Daca indexul e -1, adaug topicul in vector, altfel iau topicul de la indexul gasit si il trimit catre
toti abonatii lui.

Wildcards:
    Daca parametrul topic_can_be_wildcard este true in get_topic_index, caut fix numele topicului in topics
(cazul subscribe/unsubscribe). Altfel, atunci can trimit mai departe un mesaj UDP catre clienti, folosesc functia
matches_wildcard_path pentru comparare. Aceasta ia topicul cautat si un wildcard, transforma caracterele /,+ si * din
wildcard pentru a obtine un regex valid, si face match pe regex.

Stablilirea unei conexiuni cu clientul:
    Dacă se clientul solicita o noua conexiune prin accept, atunci voi cauta ID-ul sau in hasmap-ul clientilor.
Dacă nu gasesc ID-ul, creez un nou client, completez datele din struct si il adaug in 2 hashmapuri
(unul avand id-uri drept chei si altul fd-urile socketilor). Daca clientul e deja adaugat, ma uit daca e online
sau offline. Daca clientul este online, inseamna ca incerc sa ma conectez de 2 ori cu un client cu acelasi id, deci
trebuie sa inchid socketul. Daca era offline, doar voi schimba informatiile clientului și starea lui în online.
    La final printez mesajul care anunta noua conexiune la server.

Deconectarea clientului:
    Daca socketul clientului este gasit in hashmap, setez is_online pe false, il sterg din hashmapuri, inchid socketul
si printez un mesaj de deconectare la server.

Primirea unei comenzi de la un client TCP (subscriber):
    Daca mesajul este „exit”, atunci clientul va fi deconectat. In caz contrar, mesajul este imparti in cuvinte si
procesat. Dacă primul cuvant e „subscribe”, atunci clientul va fi abonat la topicul dat. Daca primul cuvant e
"unsubscribe", atunci clientul va fi dezabonat de la topicul dat.

Abonarea clientului la un topic:
    Caut indexul topicului in vectorul topics folosind get_topic_index, fara sa folosesc wildcards.
Daca indexul e -1, nu am gasit topicul, deci il adaug in vector. Altfel adaug clientul la vectorul de subscriberi ai
topicului gasit.

Dezabonarea clientului de la un topic:
    Caut indexul topicului in vectorul topics folosind get_topic_index, fara sa folosesc wildcards.
Daca am gasit topicul in vector, sterg clientul din vectorul de subscriberi ai topicului gasit.
