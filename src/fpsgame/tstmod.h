#ifndef __TSTMOD_H__
#define __TSTMOD_H__
#define TSTMOD 1
#endif


#ifdef SERVMODE // for server.cpp

VAR(maxtraitors, 1, 1, 2);
extern void suicide(clientinfo *ci); // zeromod

struct tstservmode : servmode
{
    static const int STARTMILLIS = 5000;

    bool roundstarted = false;
    clientinfo *ci;
    

    int countclients()
    {
        int count = 0;
        loopv(clients) if(clients[i]->state.state == CS_ALIVE /*&& !clients[i]->xi.afk*/) ++count;
        return count;
    }

    int counttraitors()
    {
        int count = 0;
        loopv(clients) if(clients[i]->state.state == CS_ALIVE && clients[i]->state.istraitor  && !clients[i]->xi.afk) ++count;
        return count;
    }

    int countinnocents()
    {
        int count = 0;
        loopv(clients) if(clients[i]->state.state == CS_ALIVE && !clients[i]->state.istraitor /*&& !clients[i]->xi.afk*/) ++count; // innocent afk?
        return count;
    }

    void endcheck()
    {
        if(!counttraitors() || !countinnocents()) startintermission();
    }

    void update()
    {
        if(gamemillis < STARTMILLIS) return;
        if(roundstarted) endcheck();
        else if(countinnocents() + counttraitors() > 1) startround();
    }

    void reset()
    {
        roundstarted = false;
        loopv(clients) clients[i]->state.istraitor = false;
    }

    void setup()
    {
        reset();
    }

    void cleanup()
    {
        reset();
    }

    void died(clientinfo *victim, clientinfo *actor)
    {
        if(roundstarted) endcheck();
        if(victim->state.istraitor)
        {
            if(actor) sendservmsgf("The traitor \fs\f3%s\fr has been killed by \fs\f1%s\fr.", colorname(victim), colorname(actor));
            else sendservmsgf("The traitor \fs\f3%s\fr committed suicide.", colorname(victim));
        }
        else if(actor && !actor->state.istraitor)
        {
            suicide(actor);
            sendf(actor->clientnum, 1, "ris", N_SERVMSG, "You killed an \fs\f1innocent\fr!");
        }
    }

    bool canspawn(clientinfo *ci, bool connecting = false)
    {
        return !roundstarted;
    }

    void leavegame(clientinfo *ci)
    {
        if(ci->state.istraitor) sendservmsgf("The traitor \fs\f3%s\fr left the game.", colorname(ci));
    }

    void intermission()
    {
        if(!counttraitors())
        {
            if(!countinnocents()) sendservmsg("Only ghosts remain...");
            else sendservmsgf("There are no traitors left. The innocents won.");
        }
        else
        {
            string names;
            int numtraitors = formattraitornames(names, MAXSTRLEN);
            if(!countinnocents()) sendservmsgf("The traitor%s %s won.", numtraitors > 1 ? "s" : "", names);
            else sendservmsgf("The traitor%s %s failed to kill all the innocents on time.", numtraitors > 1 ? "s" : "", names);
        }
    }

    void maketraitor(clientinfo *ci)
    {
        ci->state.istraitor = true;
        sendf(ci->clientnum, 1, "ris", N_SERVMSG, "You are a \fs\f3traitor\fr! Kill everyone without being noticed.");
    }

	void startround()
    {
        roundstarted = true;
        vector<clientinfo *> candidates;
        loopv(clients) if(clients[i]->state.state == CS_ALIVE && !clients[i]->state.istraitor && !clients[i]->xi.afk) candidates.add(clients[i]);
        //int numdraws = min(countclients() > 10 ? 2 : 1, countinnocents() - 1);
        int numdraws = countclients() <= 7 ? 1 : (countclients() >= 8 && countclients() <= 10 ? ((rand() % 2) + 1) : 2); // random traitorcount for 7-10
        numdraws = min(numdraws, countinnocents() - 1); // possible TODO: servsided toggleable?
        while(numdraws-- > 0 && !candidates.empty())
        {
            int i = rnd(candidates.length());
            conoutf("traitor chosen: %s", colorname(candidates[i]));
            maketraitor(candidates[i]);
            candidates.removeunordered(i);
        }

        loopv(clients)
        {
            if(!clients[i]->state.istraitor) sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, "You are \fs\f1innocent\fr! Find and kill the traitor!");
            else loopvj(clients)
            {
            if(clients[j]==clients[i] || !clients[j]->state.istraitor) continue;
                defformatstring(msg, "Your fellow traitor is \fs\f3%s\fr.", clients[j]->name);
                sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, msg);
            }
        }
    }

    int formattraitornames(char *s, int slen)
    {
        s[0] = '\0';
        vector<clientinfo *> traitors;
        loopv(clients) if(clients[i]->state.istraitor) traitors.add(clients[i]);
        for(int i = 0; i < traitors.length(); ++i)
        {
            defformatstring(name, "\fs\f%d%s\fr", traitors[i]->state.state == CS_ALIVE ? 3 : 4, colorname(traitors[i]));
            concatstring(s, name, slen);
            if(i <  traitors.length() - 2) concatstring(s, ", ", slen);
            if(i == traitors.length() - 2) concatstring(s, " and ", slen);
        }
        return traitors.length();
    }

};

#endif
