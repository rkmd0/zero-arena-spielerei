#ifdef Z_GENERICSERVERCOMMANDS_H
#error "already z_genericservercommands.h"
#endif
#define Z_GENERICSERVERCOMMANDS_H 1

#ifndef Z_SERVCMD_H
#error "want z_servcmd.h"
#endif
#ifndef Z_FORMAT_H
#error "want z_format.h"
#endif
#ifndef Z_INVPRIV_H
#error "want z_invpriv.h"
#endif


static char z_privcolor(int priv)
{
    if(priv <= PRIV_NONE) return '7';
    else if(priv <= PRIV_AUTH) return '0';
    else return '6';
}

static void z_servcmd_commands(int argc, char **argv, int sender)
{
    vector<char> cbufs[PRIV_ADMIN+1];
    clientinfo *ci = getinfo(sender);
    loopv(z_servcommands)
    {
        z_servcmdinfo &c = z_servcommands[i];
        if(!c.cansee(ci->privilege, ci->local)) continue;
        int j = 0;
        if(c.privilege >= PRIV_ADMIN) j = PRIV_ADMIN;
        else if(c.privilege >= PRIV_AUTH) j = PRIV_AUTH;
        else if(c.privilege >= PRIV_MASTER) j = PRIV_MASTER;
        if(cbufs[j].empty()) { cbufs[j].add('\f'); cbufs[j].add(z_privcolor(j)); }
        else { cbufs[j].add(','); cbufs[j].add(' '); }
        cbufs[j].put(c.name, strlen(c.name));
    }
    sendf(sender, 1, "ris", N_SERVMSG, "\f2available server commands:");
    loopi(sizeof(cbufs)/sizeof(cbufs[0]))
    {
        if(cbufs[i].empty()) continue;
        cbufs[i].add('\0');
        sendf(sender, 1, "ris", N_SERVMSG, cbufs[i].getbuf());
    }
}
SCOMMANDA(commands, PRIV_NONE, z_servcmd_commands, 1);
SCOMMANDAH(help, PRIV_NONE, z_servcmd_commands, 1);

// just testing, not really needed + ideally, you'd like to use a different file for this
#define SERVERMOD "zeromod" 
#define MOD_NAME "'traitor-version'"
#define COMMIT "r6842c"
#include <time.h>

void z_servcmd_info(int argc, char **argv, int sender)
{
    // this part is dumb but i wasnt happy with __DATE__ eeh
    time_t currentTime = time(NULL);
    struct tm* timeInfo = localtime(&currentTime);
    char formattedDate[50];
    strftime(formattedDate, sizeof(formattedDate), "%d-%m-%Y", timeInfo);

    vector<char> uptimebuf;
    z_formatsecs(uptimebuf, totalsecs);
    uptimebuf.add('\0');
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("server location: Ashburn, Virginia, USA"));
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("server version: %s %s (%s) - build %s %s", SERVERMOD, MOD_NAME, COMMIT, formattedDate, __TIME__));
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("server uptime: %s", uptimebuf.getbuf()));
}
SCOMMANDAH(info, PRIV_NONE, z_servcmd_info, 1);
SCOMMANDA(uptime, PRIV_NONE, z_servcmd_info, 1);

void z_servcmd_ignore(int argc, char **argv, int sender)
{
    if(argc <= 1) { z_servcmd_pleasespecifyclient(sender); return; }

    clientinfo *sci = getinfo(sender);
    if(!sci) return;

    bool val = strcasecmp(argv[0], "unignore")!=0;

    loopi(argc-1)
    {
        int cn;
        if(!z_parseclient_verify(argv[i+1], cn, false, true))
        {
            z_servcmd_unknownclient(argv[i+1], sender);
            continue;
        }
        if(val)
        {
            sci->xi.ignores.addunique((uchar)cn);
            sci->xi.allows.removeobj((uchar)cn);
        }
        else sci->xi.ignores.removeobj((uchar)cn);
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("%s server messages from %s", val ? "ignoring" : "unignoring", colorname(getinfo(cn))));
    }
}
SCOMMAND(ignore, PRIV_NONE, z_servcmd_ignore);
SCOMMAND(unignore, PRIV_NONE, z_servcmd_ignore);

VAR(servcmd_pm_comfirmation, 0, 1, 1);
void z_servcmd_pm(int argc, char **argv, int sender)
{
    if(argc <= 1) { z_servcmd_pleasespecifyclient(sender); return; }
    if(argc <= 2) { z_servcmd_pleasespecifymessage(sender); return; }
    int cn;
    clientinfo *sci, *ci;

    sci = getinfo(sender);
    if(!sci) return;
    if(z_checkchatmute(sci))
    {
        sendf(sender, 1, "ris", N_SERVMSG, "your pms are muted");
        return;
    }

    ci = z_parseclient_return(argv[1], false, true);
    if(!ci)
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    cn = ci->clientnum;

    bool foundallows = ci->xi.allows.find((uchar)sender) >= 0;
    if(!foundallows && ci->spy && sci->privilege < PRIV_ADMIN)
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }

    sci->xi.allows.addunique((uchar)cn);

    if(ci->xi.ignores.find((uchar)sender) >= 0)
    {
        sendf(sender, 1, "ris", N_SERVMSG, "your pms are ignored");
        return;
    }

    if(!foundallows) ci->xi.allows.add((uchar)sender);

    //sendf(cn, 1, "ris", N_SERVMSG, tempformatstring("\f6pm: \f7%s \f5(%d)\f7: \f0%s", sci->name, sci->clientnum, argv[2]));
    sendf(cn, 1, "riis", N_SAYTEAM, cn, tempformatstring("<%s [#pm %d]> %s", sci->name, sci->clientnum, argv[2]));

    if(servcmd_pm_comfirmation && strcasecmp(argv[0], "qpm") != 0)
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("your pm was successfully sent to %s \f5(%d)", ci->name, ci->clientnum));
    }
}
SCOMMANDA(pm, PRIV_NONE, z_servcmd_pm, 2);
SCOMMANDA(qpm, PRIV_NONE, z_servcmd_pm, 2);

void z_servcmd_interm(int argc, char **argv, int sender)
{
    startintermission();
}
SCOMMANDAH(interm, PRIV_MASTER, z_servcmd_interm, 1);
SCOMMANDA(intermission, PRIV_MASTER, z_servcmd_interm, 1);

void z_servcmd_wall(int argc, char **argv, int sender)
{
    if(argc <= 1) { z_servcmd_pleasespecifymessage(sender); return; }
    sendservmsg(argv[1]);
}
SCOMMANDA(wall, PRIV_ADMIN, z_servcmd_wall, 1);

void z_servcmd_achat(int argc, char **argv, int sender)
{
    if(argc <= 1) { z_servcmd_pleasespecifymessage(sender); return; }
    clientinfo *ci = getinfo(sender);
    z_servcmdinfo *c = z_servcmd_find(argv[0]);
    int priv = c ? c->privilege : PRIV_ADMIN;
    loopv(clients) if(clients[i]->state.aitype==AI_NONE && (clients[i]->local || clients[i]->privilege >= priv))
    {
        sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, tempformatstring("\f6achat: \f7%s: \f0%s", colorname(ci), argv[1]));
    }
}
SCOMMANDA(achat, PRIV_ADMIN, z_servcmd_achat, 1);

void z_servcmd_reqauth(int argc, char **argv, int sender)
{
    if(argc <= 1) { z_servcmd_pleasespecifyclient(sender); return; }
    if(argc <= 2 && !*serverauth) { sendf(sender, 1, "ris", N_SERVMSG, "please specify authdesc"); return; }

    int cn;
    if(!z_parseclient_verify(argv[1], cn, true, false, true))
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }

    const char *authdesc = argc > 2 ? argv[2] : serverauth;

    if(cn >= 0) sendf(cn, 1, "ris", N_REQAUTH, authdesc);
    else loopv(clients) if(clients[i]->state.aitype==AI_NONE) sendf(clients[i]->clientnum, 1, "ris", N_REQAUTH, authdesc);
}
SCOMMANDA(reqauth, PRIV_ADMIN, z_servcmd_reqauth, 2);

static void z_quitwhenempty_trigger(int type)
{
    if(quitwhenempty) quitserver = true;
}
Z_TRIGGER(z_quitwhenempty_trigger, Z_TRIGGER_NOCLIENTS);


// a server command called 'spec' that works like 'invpriv'
// players can assign themselves to either 'spec enabled' or 'spec disabled'
// players that got the extrainfo 'spec' will be autospecced once dead

void z_servcmd_spec(int argc, char **argv, int sender)
{
    clientinfo *ci = getinfo(sender);
    int val = (argc >= 2 && argv[1] && *argv[1]) ? clamp(atoi(argv[1]), 0, 1) : -1;
    if(val >= 0)
    {
        ci->xi.spec = val!=0;
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("spec %s", val ? "enabled" : "disabled"));

        if (val == 0 && ci->state.state == CS_SPECTATOR) unspectate(ci);
    }

    else sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("spec is %s", ci->xi.spec ? "enabled" : "disabled"));
}
SCOMMANDA(spec, PRIV_NONE, z_servcmd_spec, 1);
SCOMMANDAH(pp, PRIV_NONE, z_servcmd_spec, 1);


// a server command called 'afk' that gives the respective player the extrainfo 'afk'
// only auths can assign this role to a player, there they will be able to either 'afk enabled' or 'afk disabled'
// works like the command 'slay'
// players that got the extrainfo 'afk' will be ignored from the traitor count


void z_servcmd_afk(int argc, char **argv, int sender) 
{
    if(argc < 2) { z_servcmd_pleasespecifyclient(sender); return; }
    int cn;
    bool val = !strcasecmp(argv[0], "afk");
    if(!z_parseclient_verify(argv[1], cn, !val))
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    clientinfo *ci = cn >= 0 ? getinfo(cn) : NULL;
    if(ci && ci->local) { sendf(sender, 1, "ris", N_SERVMSG, "afk'ing local client is not allowed"); return; }
    if(ci)
    {
        ci->xi.afk = val;
        sendf(-1, 1, "ris", N_SERVMSG, tempformatstring("\fs%s\fr has been %s", colorname(ci), val ? "afk'ed and won't be able to play as traitor until back!" : "unafk'ed, welcome back!"));

    }
    else loopv(clients) if(clients[i]->state.aitype==AI_NONE && !clients[i]->local && !clients[i]->spy)
    {
        clients[i]->xi.afk = val;
        sendf(-1, 1, "ris", N_SERVMSG, tempformatstring("\fs%s\fr has been %s", colorname(ci), val ? "afk'ed and won't be able to play as traitor until back!" : "unafk'ed, welcome back!"));

    }
}

SCOMMANDA(afk, PRIV_AUTH, z_servcmd_afk, 1);
SCOMMANDA(unafk, PRIV_AUTH, z_servcmd_afk, 1);

// players should be able to put themself in afk mode
// afkme command works like spec, you get to enable/disable it via the command
// if you are afk'ed you won't be able to play as traitor until you are back

void z_servcmd_afkme(int argc, char **argv, int sender) 
{
    clientinfo *ci = getinfo(sender);
    int val = (argc >= 2 && argv[1] && *argv[1]) ? clamp(atoi(argv[1]), 0, 1) : -1;
    if(val >= 0)
    {
        ci->xi.afk = val!=0;
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("afk %s", val ? "enabled" : "disabled"));
    }
    else sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("afk is %s", ci->xi.afk ? "enabled" : "disabled"));
}
SCOMMANDAH(afkme, PRIV_NONE, z_servcmd_afkme, 1);


// command that informs the respective player of their role
// if state->istraitor true then the player is a traitor
// if state->istraitor false then the player is innocent
// if state->state == CS_SPECTATOR then the player is a spectator and the command will return an error message

// realised, counting the clients is not necessary



//  existence of a traitor implies the start of the game
/* bool hasstarted()
{
    loopv(clients)
    {
        if (clients[i]->state.state == CS_ALIVE && clients[i]->state.istraitor)
        {
            return true;
        }
    }
    return false;
} */


void z_servcmd_role(int argc, char **argv, int sender)
{
    clientinfo *ci = getinfo(sender);
    //int count = countclients();
    bool gamestarted = hasstarted();
    if(ci->state.state == CS_SPECTATOR) { sendf(sender, 1, "ris", N_SERVMSG, "spectators don't have a role"); return; }
    if(ci->state.state == CS_DEAD) { sendf(sender, 1, "ris", N_SERVMSG, "dead players don't have a role"); return; }
    if(ci->xi.afk) { sendf(sender, 1, "ris", N_SERVMSG, "you are marked as afk, you won't be able to play as traitor"); return; }
    if(ci->state.state == CS_ALIVE && gamestarted /* gamemillis >= 5000 && count >= 2 */ ) {

        if(ci->state.istraitor) { sendf(sender, 1, "ris", N_SERVMSG, "you are a traitor"); return; }
        else { sendf(sender, 1, "ris", N_SERVMSG, "you are innocent"); return; }
    }
    else { sendf(sender, 1, "ris", N_SERVMSG, "waiting for the game to start..."); return; }
    

}
SCOMMANDA(role, PRIV_NONE, z_servcmd_role, 1);


//a command that simply sendf's the rules to the sender
void z_servcmd_rules(int argc, char **argv, int sender)
{

sendf(sender, 1, "ris", N_SERVMSG, "Rules:\tTrouble in SauerTown is a game mode where players are divided into Innocents and Traitors. The objective varies depending on your role.");
sendf(sender, 1, "ris", N_SERVMSG, "\f1Innocents\f7:\tYour goal is to survive and identify the \f3Traitors\f7. Work together with other \f1Innocents\f7 and be cautious of suspicious behavior.");
//sendf(sender, 1, "ris", N_SERVMSG, "\t\tPlease note that there is a death penalty for \f1Innocent\f7-teamkills. Excessive teamkilling on purpose will be punished with a penalty.");
sendf(sender, 1, "ris", N_SERVMSG, "\f3Traitors\f7:\tEliminate \f1Innocents\f7 without being detected. Coordinate with other \f3Traitors\f7 to increase your chances of success.");
sendf(sender, 1, "ris", N_SERVMSG, "Remember to check out Server Notifications!");
}

SCOMMANDAH(rules, PRIV_NONE, z_servcmd_rules, 1);
