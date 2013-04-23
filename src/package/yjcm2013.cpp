#include "yjcm2013.h"
#include "settings.h"
#include "skill.h"
#include "standard.h"
#include "client.h"
#include "clientplayer.h"
#include "engine.h"
#include "maneuvering.h"

class Chengxiang: public MasochismSkill {
public:
    Chengxiang(): MasochismSkill("chengxiang") {
    }

    virtual void onDamaged(ServerPlayer *target, const DamageStruct &damage) const{
        Room *room = target->getRoom();
        while (room->askForSkillInvoke(target, objectName(), QVariant::fromValue(damage))) {
            if (!target->isKongcheng())
                room->showAllCards(target);
            int num = 0;
            foreach (const Card *card, target->getHandcards())
                num += card->getNumber();
            if (num >= 13) break;
            target->drawCards(1);
        }
    }
};

RenxinCard::RenxinCard() {
    target_fixed = true;
}

void RenxinCard::use(Room *room, ServerPlayer *player, QList<ServerPlayer *> &) const{
    ServerPlayer *who = room->getCurrentDyingPlayer();
    if (!who) return;

    player->turnOver();
    room->obtainCard(who, player->wholeHandCards(), false);

    RecoverStruct recover;
    recover.who = player;
    room->recover(who, recover);
}

class Renxin: public ZeroCardViewAsSkill {
public:
    Renxin(): ZeroCardViewAsSkill("renxin") {
    }

    virtual bool isEnabledAtPlay(const Player *) const{
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const{
        return pattern == "peach" && !player->isKongcheng();
    }

    virtual const Card *viewAs() const{
        return new RenxinCard;
    }
};

class Jingce: public TriggerSkill {
public:
    Jingce(): TriggerSkill("jingce") {
        events << PreCardUsed << CardResponded << EventPhaseStart << EventPhaseEnd;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if ((event == PreCardUsed || event == CardResponded) && player->getPhase() <= Player::Play) {
            CardStar card = NULL;
            if (event == PreCardUsed)
                card = data.value<CardUseStruct>().card;
            else {
                CardResponseStruct response = data.value<CardResponseStruct>();
                if (response.m_isUse)
                   card = response.m_card;
            }
            if (card && card->getHandlingMethod() == Card::MethodUse)
                player->addMark(objectName());
        } else if (event == EventPhaseStart && player->getPhase() == Player::RoundStart) {
                player->setMark(objectName(), 0);
        } else if (event == EventPhaseEnd) {
            if (player->getPhase() == Player::Play && player->getMark(objectName()) > player->getHp()) {
                if (room->askForSkillInvoke(player, objectName())) {
                    if (player->isWounded() && room->askForChoice(player, objectName(), "draw+recover") == "recover") {
                        RecoverStruct recover;
                        recover.who = player;
                        room->recover(player, recover);
                    } else {
                        player->drawCards(1);
                    }
                }
            }
        }
        return false;
    }
};

JunxingCard::JunxingCard() {
}

void JunxingCard::use(Room *room, ServerPlayer *, QList<ServerPlayer *> &targets) const{
    ServerPlayer *target = targets.first();
    if (!target->isAlive()) return;

    static QString type_name[4] = { QString(), "BasicCard", "TrickCard", "EquipCard" };
    QStringList types;
    types << "BasicCard" << "TrickCard" << "EquipCard";
    foreach (int id, subcards) {
        const Card *c = Sanguosha->getCard(id);
        types.removeOne(type_name[c->getTypeId()]);
        if (types.isEmpty()) break;
    }
    if (target->isKongcheng() || types.isEmpty()
        || !room->askForCard(target, types.join(",") + "|.|.|hand", "@junxing-discard")) {
        target->turnOver();
        target->drawCards(subcards.length(), "junxing");
    }
}

class Junxing: public ViewAsSkill {
public:
    Junxing(): ViewAsSkill("junxing") {
    }

    virtual bool viewFilter(const QList<const Card *> &, const Card *to_select) const{
        return !to_select->isEquipped() && !Self->isJilei(to_select);
    }

    virtual const Card *viewAs(const QList<const Card *> &cards) const{
        if (cards.isEmpty())
            return NULL;

        JunxingCard *card = new JunxingCard;
        card->addSubcards(cards);
        card->setSkillName(objectName());
        return card;
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return !player->isKongcheng() && !player->hasUsed("JunxingCard");
    }
};

class Yuce: public MasochismSkill {
public:
    Yuce(): MasochismSkill("yuce") {
    }

    virtual void onDamaged(ServerPlayer *target, const DamageStruct &damage) const{
        if (target->isKongcheng()) return;

        Room *room = target->getRoom();
        QVariant data = QVariant::fromValue(damage);
        const Card *card = room->askForCard(target, ".", "@yuce-show", data, Card::MethodNone);
        if (card) {
            room->notifySkillInvoked(target, objectName());
            LogMessage log;
            log.from = target;
            log.type = "#InvokeSkill";
            log.arg = objectName();
            room->sendLog(log);

            room->showCard(target, card->getEffectiveId());
            if (!damage.from || damage.from->isDead()) return;

            static QString type_name[4] = { QString(), "BasicCard", "TrickCard", "EquipCard" };
            QStringList types;
            types << "BasicCard" << "TrickCard" << "EquipCard";
            types.removeOne(type_name[card->getTypeId()]);
            if (damage.from->isKongcheng()
                || !room->askForCard(damage.from, types.join(",") + "|.|.|hand",
                                     QString("@yuce-discard:::%1:%2").arg(types.first()).arg(types.last()), data)) {
                RecoverStruct recover;
                recover.who = target;
                room->recover(target, recover);
            }
        }
    }
};

class Longyin: public TriggerSkill {
public:
    Longyin(): TriggerSkill("longyin") {
        events << CardUsed;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target->getPhase() == Player::Play;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("Slash")) {
            ServerPlayer *guanping = room->findPlayerBySkillName(objectName());
            if (guanping && !guanping->isKongcheng()
                && room->askForCard(guanping, "..", "@longyin", data, objectName())) {
                if (use.m_addHistory)
                    room->addPlayerHistory(player, use.card->getClassName(), -1);
                if (use.card->isRed())
                    guanping->drawCards(1);
            }
        }
        return false;
    }
};

ExtraCollateralCard::ExtraCollateralCard() {
}

bool ExtraCollateralCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    const Card *coll = NULL;
    foreach (QString flag, Self->getFlagList()) {
        if (flag.startsWith("ExtraCollateral:")) {
            flag = flag.mid(16);
            coll = Card::Parse(flag);
            break;
        }
    }
    QStringList tos;
    foreach (QString flag, Self->getFlagList()) {
        if (flag.startsWith("ExtraCollateralCurrentTargets:")) {
            flag = flag.mid(30);
            tos = flag.split("+");
            break;
        }
    }

    if (targets.isEmpty())
        return !tos.contains(to_select->objectName())
               && !Self->isProhibited(to_select, coll) && coll->targetFilter(targets, to_select, Self);
    else
        return coll->targetFilter(targets, to_select, Self);
}

void ExtraCollateralCard::onUse(Room *, const CardUseStruct &card_use) const{
    Q_ASSERT(card_use.to.length() == 2);
    ServerPlayer *killer = card_use.to.first();
    ServerPlayer *victim = card_use.to.last();

    killer->setFlags("ExtraCollateralTarget");
    killer->tag["collateralVictim"] = QVariant::fromValue((PlayerStar)victim);
}

QiaoshuiCard::QiaoshuiCard() {
}

bool QiaoshuiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    return targets.isEmpty() && !to_select->isKongcheng() && to_select != Self;
}

void QiaoshuiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const{
    bool success = source->pindian(targets.first(), "qiaoshui", NULL);
    if (success)
        source->setFlags("QiaoshuiSuccess");
    else
        room->setPlayerCardLimitation(source, "use", "TrickCard", true);
}

class QiaoshuiViewAsSkill: public ZeroCardViewAsSkill {
public:
    QiaoshuiViewAsSkill(): ZeroCardViewAsSkill("qiaoshui") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return !player->hasUsed("QiaoshuiCard") && !player->isKongcheng();
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const{
        return pattern == "@@qiaoshui!";
    }

    virtual const Card *viewAs() const{
        QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
        if (pattern == "@@qiaoshui!")
            return new ExtraCollateralCard;
        else
            return new QiaoshuiCard;
    }
};

class Qiaoshui: public TriggerSkill {
public:
    Qiaoshui(): TriggerSkill("qiaoshui") {
        events << PreCardUsed;
        view_as_skill = new QiaoshuiViewAsSkill;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *jianyong, QVariant &data) const{
        if (!jianyong->hasFlag("QiaoshuiSuccess")) return false;

        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isNDTrick() || use.card->isKindOf("BasicCard")) {
            jianyong->setFlags("-QiaoshuiSuccess");
            if (Sanguosha->currentRoomState()->getCurrentCardUseReason() != CardUseStruct::CARD_USE_REASON_PLAY)
                return false;

            QList<ServerPlayer *> available_targets;
            if (!use.card->isKindOf("AOE") && !use.card->isKindOf("GlobalEffect")) {
                room->setPlayerFlag(jianyong, "QiaoshuiExtraTarget");
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (use.to.contains(p) || room->isProhibited(jianyong, p, use.card)) continue;
                    if (use.card->targetFixed()) {
                        if (!use.card->isKindOf("Peach") || p->isWounded())
                            available_targets << p;
                    } else {
                        if (use.card->targetFilter(QList<const Player *>(), p, jianyong))
                            available_targets << p;
                    }
                }
                room->setPlayerFlag(jianyong, "-QiaoshuiExtraTarget");
            }
            QStringList choices;
            choices << "cancel";
            if (use.to.length() > 1) choices.prepend("remove");
            if (!available_targets.isEmpty()) choices.prepend("add");
            if (choices.length() == 1) return false;

            QString choice = room->askForChoice(jianyong, objectName(), choices.join("+"), data);
            if (choice == "cancel")
                return false;
            else if (choice == "add") {
                ServerPlayer *extra = NULL;
                if (!use.card->isKindOf("Collateral"))
                    extra = room->askForPlayerChosen(jianyong, available_targets, objectName(), "@qiaoshui-add:::" + use.card->objectName());
                else {
                    QString flag = "ExtraCollateral:" + use.card->toString();
                    QStringList tos;
                    foreach (ServerPlayer *t, use.to)
                        tos.append(t->objectName());
                    QString flag2 = "ExtraCollateralCurrentTargets:" + tos.join("+");
                    room->setPlayerFlag(jianyong, flag);
                    room->setPlayerFlag(jianyong, flag2);
                    room->askForUseCard(jianyong, "@@qiaoshui!", "@qiaoshui-add:::collateral");
                    room->setPlayerFlag(jianyong, "-" + flag);
                    room->setPlayerFlag(jianyong, "-" + flag2);
                    foreach (ServerPlayer *p, room->getOtherPlayers(jianyong)) {
                        if (p->hasFlag("ExtraCollateralTarget")) {
                            p->setFlags("-ExtraCollateralTarget");
                            extra = p;
                            break;
                        }
                    }
                    if (extra == NULL) {
                        extra = available_targets.at(qrand() % available_targets.length() - 1);
                        QList<ServerPlayer *> victims;
                        foreach (ServerPlayer *p, room->getOtherPlayers(extra)) {
                            if (extra->canSlash(p)
                                && (!(p == jianyong && p->hasSkill("kongcheng") && p->isLastHandCard(use.card, true)))) {
                                victims << p;
                            }
                        }
                        Q_ASSERT(!victims.isEmpty());
                        extra->tag["collateralVictim"] = QVariant::fromValue((PlayerStar)(victims.at(qrand() % victims.length() - 1)));
                    }
                }
                use.to.append(extra);
                room->sortByActionOrder(use.to);

                LogMessage log;
                log.type = "#QiaoshuiAdd";
                log.from = jianyong;
                log.to << extra;
                log.arg = use.card->objectName();
                log.arg2 = objectName();
                room->sendLog(log);
                room->broadcastInvoke("animate", QString("indicate:%1:%2").arg(jianyong->objectName()).arg(extra->objectName()));

                if (use.card->isKindOf("Collateral")) {
                    ServerPlayer *victim = extra->tag["collateralVictim"].value<PlayerStar>();
                    if (victim) {
                        LogMessage log;
                        log.type = "#CollateralSlash";
                        log.from = jianyong;
                        log.to << victim;
                        room->sendLog(log);
                        room->broadcastInvoke("animate", QString("indicate:%1:%2").arg(extra->objectName()).arg(victim->objectName()));
                    }
                }
            } else {
                ServerPlayer *removed = room->askForPlayerChosen(jianyong, use.to, objectName(), "@qiaoshui-remove:::" + use.card->objectName());
                use.to.removeOne(removed);

                LogMessage log;
                log.type = "#QiaoshuiRemove";
                log.from = jianyong;
                log.to << removed;
                log.arg = use.card->objectName();
                log.arg2 = objectName();
                room->sendLog(log);
            }
        }
        data = QVariant::fromValue(use);

        return false;
    }
};

class QiaoshuiTargetMod: public TargetModSkill {
public:
    QiaoshuiTargetMod(): TargetModSkill("#qiaoshui-target") {
        frequency = NotFrequent;
        pattern = "Slash,TrickCard+^DelayedTrick";
    }

    virtual int getDistanceLimit(const Player *from, const Card *) const{
        if (from->hasFlag("QiaoshuiExtraTarget"))
            return 1000;
        else
            return 0;
    }
};

class Zongshih: public TriggerSkill {
public:
    Zongshih(): TriggerSkill("zongshih") {
        events << Pindian;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const{
        PindianStar pindian = data.value<PindianStar>();
        if (TriggerSkill::triggerable(pindian->from) && room->askForSkillInvoke(pindian->from, objectName(), data)) {
            if (pindian->from_number > pindian->to_number)
                pindian->from->obtainCard(pindian->to_card);
            else
                pindian->from->obtainCard(pindian->from_card);
        } else if (TriggerSkill::triggerable(pindian->to) && room->askForSkillInvoke(pindian->to, objectName(), data)) {
            if (pindian->from_number < pindian->to_number)
                pindian->to->obtainCard(pindian->from_card);
            else
                pindian->to->obtainCard(pindian->to_card);
        }

        return false;
    }
};

XiansiCard::XiansiCard() {
}

bool XiansiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    return targets.length() < 2 && !to_select->isNude() && to_select != Self;
}

void XiansiCard::onEffect(const CardEffectStruct &effect) const{
    if (effect.to->isNude()) return;
    int id = effect.from->getRoom()->askForCardChosen(effect.from, effect.to, "he", "xiansi");
    effect.from->addToPile("counter", id);
}

class XiansiViewAsSkill: public ZeroCardViewAsSkill {
public:
    XiansiViewAsSkill(): ZeroCardViewAsSkill("xiansi") {
    }

    virtual bool isEnabledAtPlay(const Player *) const{
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const{
        return pattern == "@@xiansi";
    }

    virtual const Card *viewAs() const{
        return new XiansiCard;
    }
};

class Xiansi: public TriggerSkill {
public:
    Xiansi(): TriggerSkill("xiansi") {
        events << EventPhaseStart;
        view_as_skill = new XiansiViewAsSkill;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const{
        if (player->getPhase() == Player::Start)
            room->askForUseCard(player, "@@xiansi", "@xiansi-card");
        return false;
    }
};

class XiansiAttach: public TriggerSkill {
public:
    XiansiAttach(): TriggerSkill("#xiansi-attach") {
        events << GameStart << EventAcquireSkill << EventLoseSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if ((event == GameStart && TriggerSkill::triggerable(player))
             || (event == EventAcquireSkill && data.toString() == "xiansi")) {
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (!p->hasSkill("xiansi_slash"))
                    room->attachSkillToPlayer(p, "xiansi_slash");
            }
        } else if (event == EventLoseSkill && data.toString() == "xiansi") {
            player->clearOnePrivatePile("counter");
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (p->hasSkill("xiansi_slash"))
                    room->detachSkillFromPlayer(p, "xiansi_slash", true);
            }
        }
        return false;
    }
};

XiansiSlashCard::XiansiSlashCard() {
    target_fixed = true;
    m_skillName = "xiansi_slash";
}

void XiansiSlashCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const{
    ServerPlayer *liufeng = room->findPlayerBySkillName("xiansi");
    if (!liufeng || liufeng->getPile("counter").length() < 2) return;

    DummyCard *dummy = new DummyCard;
    if (liufeng->getPile("counter").length() == 2) {
        dummy->addSubcard(liufeng->getPile("counter").first());
        dummy->addSubcard(liufeng->getPile("counter").last());
    } else {
        int ai_delay = Config.AIDelay;
        Config.AIDelay = 0;

        QList<int> ids = liufeng->getPile("counter");
        for (int i = 0; i < 2; i++) {
            room->fillAG(ids, source);
            int id = room->askForAG(source, ids, false, "xiansi");
            dummy->addSubcard(id);
            ids.removeOne(id);
            room->clearAG(source);
        }

        Config.AIDelay = ai_delay;

    }

    CardMoveReason reason(CardMoveReason::S_REASON_REMOVE_FROM_PILE, QString(), "xiansi", QString());
    room->throwCard(dummy, reason, NULL);
    delete dummy;

    Slash *slash = new Slash(Card::SuitToBeDecided, -1);
    slash->setSkillName("XIANSI");
    room->useCard(CardUseStruct(slash, source, liufeng));
}

class XiansiSlashViewAsSkill: public ZeroCardViewAsSkill {
public:
    XiansiSlashViewAsSkill(): ZeroCardViewAsSkill("xiansi_slash") {
        attached_lord_skill = true;
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return Slash::IsAvailable(player) && canSlashLiufeng(player);
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const{
        return pattern == "slash" && !ClientInstance->hasNoTargetResponding()
               && canSlashLiufeng(player);
    }

    virtual const Card *viewAs() const{
        return new XiansiSlashCard;
    }

private:
    static bool canSlashLiufeng(const Player *player) {
        const Player *liufeng = NULL;
        foreach (const Player *p, player->getSiblings()) {
            if (p->isAlive() && p->hasSkill("xiansi") && p->getPile("counter").length() > 1) {
                liufeng = p;
                break;
            }
        }
        if (!liufeng) return false;

        Slash *slash = new Slash(Card::SuitToBeDecided, -1);
        slash->deleteLater();
        return slash->targetFilter(QList<const Player *>(), liufeng, player);
    }
};

DuodaoCard::DuodaoCard() {
}

bool DuodaoCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    return targets.isEmpty() && !to_select->isKongcheng() && to_select != Self && to_select->getWeapon() != NULL;
}

void DuodaoCard::use(Room *room, ServerPlayer *panma, QList<ServerPlayer *> &targets) const{
    ServerPlayer *target = targets.first();
    bool success = panma->pindian(target, "duodao", NULL);
    if (success) {
        if (target->getWeapon())
            room->obtainCard(panma, target->getWeapon());
    } else
        room->setPlayerCardLimitation(panma, "use", "Slash", true);
}

class Duodao: public ZeroCardViewAsSkill {
public:
    Duodao(): ZeroCardViewAsSkill("duodao") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return !player->hasUsed("DuodaoCard") && !player->isKongcheng();
    }

    virtual const Card *viewAs() const{
        return new DuodaoCard;
    }
};

class Anjian: public TriggerSkill {
public:
    Anjian(): TriggerSkill("anjian") {
        events << DamageCaused;
        frequency = Compulsory;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const{
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.chain || damage.transfer) return false;
        if (damage.from && !damage.to->inMyAttackRange(damage.from)
            && damage.card && damage.card->isKindOf("Slash")) {
            LogMessage log;
            log.type = "#AnjianBuff";
            log.from = damage.from;
            log.to << damage.to;
            log.arg = QString::number(damage.damage);
            log.arg2 = QString::number(++damage.damage);
            room->sendLog(log);

            data = QVariant::fromValue(damage);
        }

        return false;
    }
};

ZongxuanCard::ZongxuanCard() {
    will_throw = false;
    handling_method = Card::MethodNone;
    target_fixed = true;
}

void ZongxuanCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const{
    CardMoveReason reason(CardMoveReason::S_REASON_PUT, source->objectName(),
                          QString(), "zongxuan", QString());
    room->moveCardTo(this, source, NULL, Player::DrawPile, reason, true);
    QStringList zongxuan = source->property("zongxuan").toString().split("+");
    foreach (int id, this->subcards)
        zongxuan.removeOne(QString::number(id));
    room->setPlayerProperty(source, "zongxuan", zongxuan.join("+"));
}

class ZongxuanViewAsSkill: public ViewAsSkill {
public:
    ZongxuanViewAsSkill(): ViewAsSkill("zongxuan") {
    }

    bool isEnabledAtPlay(const Player *) const{
        return false;
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const{
        return pattern == "@@zongxuan";
    }

    bool viewFilter(const QList<const Card *> &, const Card *to_select) const{
        QStringList zongxuan = Self->property("zongxuan").toString().split("+");
        foreach (QString id, zongxuan) {
            bool ok;
            if (id.toInt(&ok) == to_select->getEffectiveId() && ok)
                return true;
        }
        return false;
    }

    const Card *viewAs(const QList<const Card *> &cards) const{
        if (cards.isEmpty()) return NULL;

        ZongxuanCard *card = new ZongxuanCard;
        card->addSubcards(cards);
        return card;
    }
};

class Zongxuan: public TriggerSkill {
public:
    Zongxuan(): TriggerSkill("zongxuan") {
        events << BeforeCardsMove;
        view_as_skill = new ZongxuanViewAsSkill;
    }

    virtual bool trigger(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data) const{
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (move.from != player)
            return false;
        if (move.to_place == Player::DiscardPile
            && ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD)) {

            int i = 0;
            QList<int> zongxuan_card;
            foreach (int card_id, move.card_ids) {
                if (room->getCardOwner(card_id) == move.from
                    && (move.from_places[i] == Player::PlaceHand || move.from_places[i] == Player::PlaceEquip)) {
                        zongxuan_card << card_id;
                }
                i++;
            }
            if (zongxuan_card.isEmpty())
                return false;

            QList<int> original_zongxuan = zongxuan_card;
            room->setPlayerProperty(player, "zongxuan", IntList2StringList(zongxuan_card).join("+"));
            do {
                if (!room->askForUseCard(player, "@@zongxuan", "@zongxuan-put")) break;
                QStringList zongxuan = player->property("zongxuan").toString().split("+");
                foreach (int id, zongxuan_card) {
                    if (!zongxuan.contains(QString::number(id)))
                        zongxuan_card.removeOne(id);
                }
            } while (!zongxuan_card.isEmpty());

            QList<int> ids = move.card_ids;
            i = 0;
            foreach (int id, ids) {
                if (original_zongxuan.contains(id) && !zongxuan_card.contains(id)) {
                    move.card_ids.removeOne(id);
                    move.from_places.removeAt(i);
                }
                i++;
            }
            data = QVariant::fromValue(move);
        }
        return false;
    }
};

class Zhiyan: public PhaseChangeSkill {
public:
    Zhiyan(): PhaseChangeSkill("zhiyan") {
    }

    virtual bool onPhaseChange(ServerPlayer *target) const{
        if (target->getPhase() != Player::Finish)
            return false;

        Room *room = target->getRoom();
        ServerPlayer *to = room->askForPlayerChosen(target, room->getAlivePlayers(), objectName(), "zhiyan-invoke", true, true);
        if (to) {
            QList<int> ids = room->getNCards(1, false);
            const Card *card = Sanguosha->getCard(ids.first());
            room->obtainCard(to, card, false);
            if (!to->isAlive())
                return false;
            room->showCard(to, ids.first());

            if (card->isKindOf("EquipCard")) {
                if (to->isWounded()) {
                    RecoverStruct recover;
                    recover.who = target;
                    room->recover(to, recover);
                }
                if (to->isAlive() && !to->isCardLimited(card, Card::MethodUse))
                    room->useCard(CardUseStruct(card, to, to));
            }
        }
        return false;
    }
};

class Danshou: public TriggerSkill {
public:
    Danshou(): TriggerSkill("danshou") {
        events << Damage;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        if (room->askForSkillInvoke(player, objectName(), data)) {
            player->drawCards(1);
            ServerPlayer *current = room->getCurrent();
            if (current && current->isAlive() && current->getPhase() != Player::NotActive) {
                LogMessage log;
                log.type = "#TurnBroken";
                log.from = current;
                room->sendLog(log);
                throw TurnBroken;
            }
        }
        return false;
    }
};

class Juece: public TriggerSkill {
public:
    Juece(): TriggerSkill("juece") {
        events << BeforeCardsMove << CardsMoveOneTime;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (player->getPhase() != Player::NotActive && move.from && move.from_places.contains(Player::PlaceHand)) {
            ServerPlayer *from = (ServerPlayer *)move.from;
            if (event == BeforeCardsMove) {
                if (from->isKongcheng() || from->getHp() < 1) return false;
                foreach (int id, from->handCards()) {
                    if (!move.card_ids.contains(id))
                        return false;
                }
                player->addMark(objectName());
            } else {
                if (player->getMark(objectName()) == 0)
                    return false;
                player->removeMark(objectName());
                if (room->askForSkillInvoke(player, objectName(), data))
                    room->damage(DamageStruct(objectName(), player, from));
            }
        }

        return false;
    }
};

class Mieji: public TargetModSkill {
public:
    Mieji(): TargetModSkill("#mieji") {
        pattern = "SingleTargetTrick|.|.|.|black"; // deal with these two later
    }

    virtual int getExtraTargetNum(const Player *from, const Card *) const{
        if (from->hasSkill("mieji"))
            return 1;
        else
            return 0;
    }
};

class MiejiViewAsSkill: public ZeroCardViewAsSkill {
public:
    MiejiViewAsSkill(): ZeroCardViewAsSkill("mieji") {
    }

    virtual bool isEnabledAtPlay(const Player *) const{
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const{
        return pattern == "@@mieji";
    }

    virtual const Card *viewAs() const{
        return new ExtraCollateralCard;
    }
};

class MiejiForExNihiloAndCollateral: public TriggerSkill {
public:
    MiejiForExNihiloAndCollateral(): TriggerSkill("mieji") {
        events << PreCardUsed;
        frequency = Compulsory;
        view_as_skill = new MiejiViewAsSkill;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isBlack() && (use.card->isKindOf("ExNihilo") || use.card->isKindOf("Collateral"))) {
            QList<ServerPlayer *> targets;
            ServerPlayer *extra = NULL;
            if (use.card->isKindOf("ExNihilo")) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (!use.to.contains(p) && !room->isProhibited(player, p, use.card))
                        targets << p;
                }
                if (targets.isEmpty()) return false;
                extra = room->askForPlayerChosen(player, targets, objectName(), "@qiaoshui-add:::" + use.card->objectName(), true);
            } else if (use.card->isKindOf("Collateral")) {
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (use.to.contains(p) || room->isProhibited(player, p, use.card)) continue;
                    if (use.card->targetFilter(QList<const Player *>(), p, player))
                        targets << p;
                }
                if (targets.isEmpty()) return false;

                QString flag = "ExtraCollateral:" + use.card->toString();
                QStringList tos;
                foreach (ServerPlayer *t, use.to)
                    tos.append(t->objectName());
                QString flag2 = "ExtraCollateralCurrentTargets:" + tos.join("+");
                room->setPlayerFlag(player, flag);
                room->setPlayerFlag(player, flag2);
                bool used = room->askForUseCard(player, "@@mieji", "@qiaoshui-add:::collateral");
                room->setPlayerFlag(player, "-" + flag);
                room->setPlayerFlag(player, "-" + flag2);
                if (!used) return false;
                foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                    if (p->hasFlag("ExtraCollateralTarget")) {
                        p->setFlags("-ExtraCollateralTarget");
                        extra = p;
                        break;
                    }
                }
            }
            if (!extra) return false;
            use.to.append(extra);
            room->sortByActionOrder(use.to);

            LogMessage log;
            log.type = "#QiaoshuiAdd";
            log.from = player;
            log.to << extra;
            log.arg = use.card->objectName();
            log.arg2 = objectName();
            room->sendLog(log);
            room->broadcastInvoke("animate", QString("indicate:%1:%2").arg(player->objectName()).arg(extra->objectName()));

            if (use.card->isKindOf("Collateral")) {
                ServerPlayer *victim = extra->tag["collateralVictim"].value<PlayerStar>();
                if (victim) {
                    LogMessage log;
                    log.type = "#CollateralSlash";
                    log.from = player;
                    log.to << victim;
                    room->sendLog(log);
                    room->broadcastInvoke("animate", QString("indicate:%1:%2").arg(extra->objectName()).arg(victim->objectName()));
                }
            }
        }
        return false;
    }
};

class Fencheng: public ZeroCardViewAsSkill {
public:
    Fencheng(): ZeroCardViewAsSkill("fencheng") {
        frequency = Limited;
    }

    virtual const Card *viewAs() const{
        return new FenchengCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return player->getMark("@burn") >= 1;
    }
};

FenchengCard::FenchengCard() {
    mute = true;
    target_fixed = true;
}

void FenchengCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const{
    source->loseMark("@burn");
    room->broadcastSkillInvoke("fencheng");
    //room->doLightbox("$FenchengAnimate", 3000);

    QList<ServerPlayer *> players = room->getOtherPlayers(source);
    source->setFlags("FenchengUsing");
    foreach (ServerPlayer *player, players) {
        if (player->isAlive())
            room->cardEffect(this, source, player);
            room->getThread()->delay();
    }
    source->setFlags("-FenchengUsing");
}

void FenchengCard::onEffect(const CardEffectStruct &effect) const{
    Room *room = effect.to->getRoom();

    int length = qMax(1, effect.to->getEquips().length());
    if (effect.to->getHandcardNum() < length || !room->askForDiscard(effect.to, "fencheng", length, length, true))
        room->damage(DamageStruct("fencheng", effect.from, effect.to, 1, DamageStruct::Fire));
}

class Zhuikong: public TriggerSkill {
public:
    Zhuikong(): TriggerSkill("zhuikong") {
        events << EventPhaseStart;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const{
        if (player->getPhase() != Player::RoundStart || player->isKongcheng())
            return false;

        bool skip = false;
        foreach (ServerPlayer *fuhuanghou, room->findPlayersBySkillName(objectName())) {
            if (player != fuhuanghou && fuhuanghou->isWounded() && !fuhuanghou->isKongcheng()
                && room->askForSkillInvoke(fuhuanghou, objectName())) {
                if (fuhuanghou->pindian(player, objectName(), NULL)) {
                    if (!skip) {
                        player->skip(Player::Play);
                        skip = true;
                    } else {
                        room->setFixedDistance(player, fuhuanghou, 1);
                        QVariantList zhuikonglist = player->tag[objectName()].toList();
                        zhuikonglist.append(QVariant::fromValue((PlayerStar)fuhuanghou));
                        player->tag[objectName()] = QVariant::fromValue(zhuikonglist);
                    }
                }
            }
        }
        return false;
    }
};

class ZhuikongClear: public TriggerSkill {
public:
    ZhuikongClear(): TriggerSkill("#zhuikong-clear") {
        events << EventPhaseChanging;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to != Player::NotActive)
            return false;

        QVariantList zhuikonglist = player->tag["zhuikong"].toList();
        if (zhuikonglist.isEmpty()) return false;
        foreach (QVariant p, zhuikonglist) {
            PlayerStar fuhuanghou = p.value<PlayerStar>();
            room->setFixedDistance(player, fuhuanghou, -1);
        }
        player->tag.remove("zhuikong");
        return false;
    }
};

class Qiuyuan: public TriggerSkill {
public:
    Qiuyuan(): TriggerSkill("qiuyuan") {
        events << TargetConfirming;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("Slash")) {
            QList<ServerPlayer *> targets;
            foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
                if (!p->isKongcheng() && p != use.from)
                    targets << p;
            }
            if (targets.isEmpty()) return false;
            ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "qiuyuan-invoke", true, true);
            if (target) {
                const Card *card = room->askForCard(target, ".!", "@qiuyuan-give:" + player->objectName(), data, Card::MethodNone);
                if (!card)
                    card = target->getHandcards().at(qrand() % target->getHandcardNum() - 1);
                player->obtainCard(card);
                room->showCard(player, card->getEffectiveId());
                if (!card->isKindOf("Jink")) {
                    if (use.from->canSlash(target, use.card, false)) {
                        use.to.append(target);
                        room->sortByActionOrder(use.to);
                        data = QVariant::fromValue(use);
                        room->getThread()->trigger(TargetConfirming, room, target, data);
                    }
                }
            }
        }
        return false;
    }
};

YJCM2013Package::YJCM2013Package()
    : Package("YJCM2013")
{
    General *caochong = new General(this, "caochong", "wei", 3);
    caochong->addSkill(new Chengxiang);
    caochong->addSkill(new Renxin);

    General *fuhuanghou = new General(this, "fuhuanghou", "qun", 3);
    fuhuanghou->addSkill(new Zhuikong);
    fuhuanghou->addSkill(new ZhuikongClear);
    fuhuanghou->addSkill(new Qiuyuan);
    related_skills.insertMulti("zhuikong", "#zhuikong-clear");

    General *guohuai = new General(this, "guohuai", "wei");
    guohuai->addSkill(new Jingce);

    General *guanping = new General(this, "guanping", "shu", 4);
    guanping->addSkill(new Longyin);

    General *jianyong = new General(this, "jianyong", "shu", 3);
    jianyong->addSkill(new Qiaoshui);
    jianyong->addSkill(new QiaoshuiTargetMod);
    jianyong->addSkill(new Zongshih);
    related_skills.insertMulti("qiaoshui", "#qiaoshui-target");

    General *liru = new General(this, "liru", "qun", 3);
    liru->addSkill(new Juece);
    liru->addSkill(new Mieji);
    liru->addSkill(new MiejiForExNihiloAndCollateral);
    liru->addSkill(new Fencheng);
    liru->addSkill(new MarkAssignSkill("@burn", 1));
    related_skills.insertMulti("mieji", "#mieji");
    related_skills.insertMulti("fencheng", "#@burn-1");

    General *liufeng = new General(this, "liufeng", "shu");
    liufeng->addSkill(new Xiansi);
    liufeng->addSkill(new XiansiAttach);
    related_skills.insertMulti("xiansi", "#xiansi-attach");

    General *manchong = new General(this, "manchong", "wei", 3);
    manchong->addSkill(new Junxing);
    manchong->addSkill(new Yuce);

    General *panzhangmazhong = new General(this, "panzhangmazhong", "wu");
    panzhangmazhong->addSkill(new Duodao);
    panzhangmazhong->addSkill(new Anjian);

    General *yufan = new General(this, "yufan", "wu", 3);
    yufan->addSkill(new Zongxuan);
    yufan->addSkill(new Zhiyan);

    General *zhuran = new General(this, "zhuran", "wu");
    zhuran->addSkill(new Danshou);

    addMetaObject<RenxinCard>();
    addMetaObject<JunxingCard>();
    addMetaObject<QiaoshuiCard>();
    addMetaObject<XiansiCard>();
    addMetaObject<XiansiSlashCard>();
    addMetaObject<DuodaoCard>();
    addMetaObject<ZongxuanCard>();
    addMetaObject<FenchengCard>();
    addMetaObject<ExtraCollateralCard>();

    skills << new XiansiSlashViewAsSkill;
}

ADD_PACKAGE(YJCM2013)
