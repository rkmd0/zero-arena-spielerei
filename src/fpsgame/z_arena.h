// src/fpsgame/z_arena.h
#ifdef Z_ARENA_H
#error "already z_arena.h"
#endif
#define Z_ARENA_H

// Quake/Clan-Arena style round-based mode implemented as a servmode.
// Toggle with the cvar `arena` (0/1).

VAR(arena, 0, 0, 1);                   // 0 = Traitor/TST (default), 1 = Clan Arena
VAR(arena_preround, 1000, 5000, 60000); // ms before FIGHT (invulnerable)
VAR(arena_postround, 500, 4000, 60000); // ms after a round ends
VAR(arena_roundtime, 0, 180000, 900000); // ms for an active round (0 = unlimited)
VAR(arena_minplayers, 2, 2, 32);       // min participants to start

// Spawn loadout (Quake-like; uses existing guns)
VAR(arena_health, 1, 125, 200);
VAR(arena_armour, 0, 100, 200);                     // A_GREEN
VAR(arena_startgun, GUN_FIST, GUN_RL, NUMGUNS-1);   // default RL
VAR(arena_ammo_sg, 0, 20, 200);
VAR(arena_ammo_cg, 0, 120, 1000);
VAR(arena_ammo_rl, 0, 20, 100);
VAR(arena_ammo_rifle, 0, 12, 200);
VAR(arena_ammo_gl, 0, 10, 200);
VAR(arena_ammo_pistol, 0, 60, 1000);
VAR(arena_disableitems, 0, 1, 1);      // hard-disable item spawns
VAR(arena_staydead, 0, 1, 1);          // one life per round

struct arenaservmode : servmode
{
    enum roundstate { RS_WAITING = 0, RS_PREROUND, RS_ACTIVE, RS_POSTROUND };
    static const int STARTMILLIS = 5000; // protect first ms of a new map

    roundstate state;
    int statechangemillis;
    int roundstartmillis;
    int roundnum;

    arenaservmode() : state(RS_WAITING), statechangemillis(0), roundstartmillis(0), roundnum(0) {}

    static bool participating(clientinfo *ci) { return ci && ci->state.state!=CS_SPECTATOR && !ci->xi.afk; }
    static bool alive(clientinfo *ci) { return ci && ci->state.state==CS_ALIVE; }

    int countalive() const
    {
        int n = 0; loopv(clients) if(participating(clients[i]) && alive(clients[i])) n++; return n;
    }
    int countteamalive(const char *team) const
    {
        int n = 0; loopv(clients){ clientinfo *ci = clients[i]; if(!participating(ci) || !alive(ci)) continue;
            if(!m_teammode || !strcmp(ci->team, team)) n++; } return n;
    }

    void setnodamage(int on) { extern int servernodamage; servernodamage = on; }

    void spawn_with_loadout(clientinfo *ci)
    {
        if(!ci) return;
        gamestate &gs = ci->state;
        sendspawn(ci); // normal spawnstate(gamemode)

        gs.health = clamp(arena_health, 1, 200);
        gs.maxhealth = gs.health;
        gs.armourtype = A_GREEN;
        gs.armour = clamp(arena_armour, 0, 200);

        gs.ammo[GUN_SG]     = arena_ammo_sg;
        gs.ammo[GUN_CG]     = arena_ammo_cg;
        gs.ammo[GUN_RL]     = arena_ammo_rl;
        gs.ammo[GUN_RIFLE]  = arena_ammo_rifle;
        gs.ammo[GUN_GL]     = arena_ammo_gl;
        gs.ammo[GUN_PISTOL] = arena_ammo_pistol;

        if(arena_startgun >= GUN_FIST && arena_startgun < NUMGUNS) gs.gunselect = arena_startgun;

        // Push overridden state to client
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_SPAWNSTATE); putint(p, ci->clientnum); sendstate(gs, p);
        sendpacket(ci->ownernum, 1, p.finalize());
    }

    void spawn_all_participants()
    {
        loopv(clients){ clientinfo *ci = clients[i];
            if(!participating(ci)) continue;
            if(ci->state.state != CS_ALIVE) spawn_with_loadout(ci);
        }
    }

    void forceroundreset()
    {
        loopv(clients){ clientinfo *ci = clients[i];
            if(!ci || ci->state.state==CS_SPECTATOR) continue;
            if(ci->state.state==CS_ALIVE)
            {
                sendf(-1, 1, "ri2", N_FORCEDEATH, ci->clientnum);
                ci->state.state = CS_DEAD;
                ci->state.statemillis = totalmillis;
            }
        }
    }

    void announce_survivors()
    {
        if(m_teammode)
        {
            int red = countteamalive("good"), blue = countteamalive("evil");
            if(!red && !blue) sendservmsg("Round over. No survivors.");
            else if(red && !blue) sendservmsg("\fs\f1Good\fr win the round!");
            else if(blue && !red) sendservmsg("\fs\f3Evil\fr win the round!");
            else sendservmsg("Round over.");
        }
        else
        {
            clientinfo *last = NULL;
            loopv(clients) if(participating(clients[i]) && alive(clients[i])) { last = clients[i]; break; }
            if(last) sendservmsgf("%s wins the round!", colorname(last));
            else sendservmsg("Round over. No survivors.");
        }
    }

    bool should_start_round() const
    {
        int have = 0; loopv(clients) if(participating(clients[i])) have++;
        if(!m_teammode) return have >= arena_minplayers;
        bool hasgood=false, hasevil=false;
        loopv(clients){ clientinfo *ci = clients[i]; if(!participating(ci)) continue;
            if(!strcmp(ci->team,"good")) hasgood=true; else if(!strcmp(ci->team,"evil")) hasevil=true; }
        return hasgood && hasevil;
    }

    // ----- servmode interface -----
    bool canspawn(clientinfo *ci, bool connecting = false)
    {
        if(!arena) return true;
        return state==RS_WAITING; // normal spawns only while waiting
    }

    void spawned(clientinfo *ci)
    {
        if(!arena) return;
        if(state==RS_PREROUND || state==RS_ACTIVE) spawn_with_loadout(ci);
    }

    void entergame(clientinfo *ci)
    {
        if(!arena) return;
        if(state==RS_ACTIVE || state==RS_PREROUND)
        {
            if(ci->state.state!=CS_SPECTATOR)
            {
                sendf(-1, 1, "ri3", N_SPECTATOR, ci->clientnum, 1);
                forcespectator(ci);
            }
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, "Waiting for round to finish...");
        }
    }

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        if(!arena) return;
        if(state==RS_ACTIVE) check_end();
    }

    void died(clientinfo *victim, clientinfo *actor)
    {
        if(!arena) return;
        if(arena_staydead && victim->state.state!=CS_SPECTATOR) victim->state.respawn();
        check_end();
    }

    void setup()
    {
        state = RS_WAITING; statechangemillis = gamemillis; roundstartmillis = 0; roundnum = 0;
        if(arena_disableitems)
        {
            loopv(sents){ sents[i].spawned = false; sents[i].spawntime = 0x7FFFFFFF; } // never spawn
        }
    }

    void cleanup() { setnodamage(0); state = RS_WAITING; }
    void intermission() { setnodamage(0); }

    void check_end()
    {
        if(state!=RS_ACTIVE) return;
        if(m_teammode)
        {
            int good = countteamalive("good"), evil = countteamalive("evil");
            if((!good && evil) || (!evil && good) || (!good && !evil))
            { announce_survivors(); state = RS_POSTROUND; statechangemillis = gamemillis; }
        }
        else
        {
            if(countalive() <= 1)
            { announce_survivors(); state = RS_POSTROUND; statechangemillis = gamemillis; }
        }
    }

    void update()
    {
        if(!arena) return;
        if(gamemillis < STARTMILLIS) return;

        switch(state)
        {
            case RS_WAITING:
                if(should_start_round())
                {
                    roundnum++; sendservmsgf("Round %d starting...", roundnum);
                    setnodamage(1); spawn_all_participants();
                    state = RS_PREROUND; statechangemillis = gamemillis;
                }
                break;
            case RS_PREROUND:
                if(gamemillis - statechangemillis >= arena_preround)
                { setnodamage(0); state = RS_ACTIVE; roundstartmillis = gamemillis; sendservmsg("\fs\f2FIGHT!\fr"); }
                break;
            case RS_ACTIVE:
                if(arena_roundtime && gamemillis - roundstartmillis >= arena_roundtime)
                { announce_survivors(); state = RS_POSTROUND; statechangemillis = gamemillis; }
                else check_end();
                break;
            case RS_POSTROUND:
                if(gamemillis - statechangemillis >= arena_postround)
                { forceroundreset(); state = RS_WAITING; statechangemillis = gamemillis; }
                break;
        }
    }

    bool hidefrags() { return arena != 0; } // optional: hide frag list while in Arena
};
