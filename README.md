# MAGE - Merely Another Gameboy Emulator

A game boy emulator written in C as a personal (and very fun) exercise.
The graphics are displayed with SDL which is aside from libc the only dependency.

It
- passes the blargg instruction tests at least
- is able to run games such as Links Awakening or Pokemon Red and Blue.

In the future I'm going to add
- linkcable support (over TCP/IP) (so one can exchange Pokemon, yes!)
- configurable settings
- sound!
- maybe even colors!

Build with cmake or without (actually its easy, 
just compile everything in src together, no fancy flags needed.).

Run with
```
    ./GameBoy --file your_game.gb
```

or
```
    ./GameBoy --help
```
for more information!