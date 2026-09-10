#include "Item.h"
#include "Player.h"
#include <random>
#include <algorithm>

InventoryItem ItemManager::CreateItem(ItemId id, int extraVal) {
    InventoryItem item;
    item.id = id;
    item.count = 1;

    switch (id) {
    case ItemId::Lantern:
        item.name = "携帯ランタン ";
        item.desc = "暗闇を照らす油火灯。手持ち枠に装備し、[L]キーで点火/消灯を行う。";
        break;
    case ItemId::OilBottle:
        item.name = "燃料ボトルの小瓶 ";
        item.desc = "ランタン用揮発油。使用すると油が補給される。";
        break;
    case ItemId::OldDiary:
        return CreateDiaryItem(extraVal > 0 ? extraVal : 1);
    case ItemId::Match: {
        item.uses = (extraVal > 0) ? extraVal : 2;
        char buf[32];
        sprintf_s(buf, "マッチ (残り%d本) ", item.uses);
        item.name = buf;
        if (item.uses == 1) item.desc = "湿気たマッチ。使えなくはないが、火種は最後の1本だ。";
        else if (item.uses == 2) item.desc = "そこそこ状態のいいマッチ。壁で擦れば火がつくだろう。";
        else item.desc = "いたって普通のマッチ。3本入っており、暗闇での頼もしい命綱だ。";
        break;
    }
    case ItemId::Shotgun:
        item.name = "古びたショットガン ";
        item.durability = (extraVal > 0) ? static_cast<float>(extraVal) : 100.0f;
        item.uses = 2; // 応急整備の可能回数 (初期2回)
        item.desc = "旧式の散弾銃。近距離のスライムを粉砕できるが、撃つたびに損耗する音がする。\n長くはもたないだろう";
        break;
    case ItemId::ShotgunShells:
        item.name = "ショットガンの散弾 ";
        item.count = (extraVal > 0) ? extraVal : 2;
        item.desc = "12ゲージ散弾。木箱の底に転がっていた実包。湿気ているが撃てないことはないだろう";
        break;
    case ItemId::GunParts:
        item.name = "銃の予備部品 ";
        item.desc = "真鍮製の撃鉄とスプリング。多少錆びているが作業台でショットガンを整備できる。";
        break;
    case ItemId::Bandage:
        item.name = "清潔な包帯 ";
        item.desc = "滅菌包装が残る包帯。裂傷を塞ぐくらいならできそうだ。";
        break;
    case ItemId::Hemostatic:
        item.name = "軍用止血剤 ";
        item.desc = "急速凝固粉末。激痛を伴うが、重傷を抑えられる";
        break;
    case ItemId::Painkiller:
        item.name = "強力な鎮痛剤 ";
        item.desc = "中枢神経に作用する錠剤。痛みを麻痺させることができる。\n使用すると少しふらふらする";
        break;
    default:
        break;
    }
    return item;
}

InventoryItem ItemManager::CreateDiaryItem(int diaryOrder) {
    InventoryItem item;
    item.id = ItemId::OldDiary;
    item.count = 1;
    item.uses = diaryOrder;

    char nameBuf[32];
    sprintf_s(nameBuf, "古びた日誌 (#%d) ", diaryOrder);
    item.name = nameBuf;

    if (diaryOrder == 1) {
        item.desc = "『走り書きのメモ』\n\n頭が割れるように痛む。ここはどこだ。崩落に巻き込まれたのか？\n周囲から冷たい湿気と異臭が漂っている。光源の油を惜しんでいる場合ではない。\nとにかく下へ続く階段を探し、早くここから脱出しないと。";
    }
    else if (diaryOrder == 2) {
        item.desc = "『調査記録の断片』\n\n通路を塞ぐ緑色の肉塊と遭遇した。強酸を滲ませながらゆっくりと蠢いている。\n動きは非常に鈍く、落ち着いて距離を取れば振り切ることは難しくない。\n迂回ルートを探すか、古びた散弾銃で核ごと粉砕するべきだ。";
    }
    else if (diaryOrder == 3) {
        item.desc = "『誰かの手記』\n\n古びた作業台、配管、非常電源で切れかけた電灯……ここはただの廃坑ではない。\nかつて何かを隔離・研究していた地下施設の跡地だ。奥へ進むほど、空気の淀みが濃くなっていく。";
    }
    else {
        item.desc = "『破れた日誌の切れ端』\n\n酸と血痕でページの大半が溶けて固まっており、文字を判読することはできない……。";
    }

    return item;
}

InventoryItem ItemManager::RollCrateDrop(std::vector<ItemId>& droppedHistory, unsigned int seed) {
    std::mt19937 rng(seed);

    if (rng() % 100 < 45) {
        InventoryItem emptyItem;
        emptyItem.id = ItemId::None;
        return emptyItem;
    }

    std::vector<ItemId> candidates = {
        ItemId::Match,
        ItemId::ShotgunShells,
        ItemId::Bandage,
        ItemId::Hemostatic,
        ItemId::Painkiller
    };

    std::vector<ItemId> available;
    for (auto c : candidates) {
        if (std::find(droppedHistory.begin(), droppedHistory.end(), c) == droppedHistory.end()) {
            available.push_back(c);
        }
    }
    if (available.empty()) available = candidates;

    ItemId selected = available[rng() % available.size()];
    droppedHistory.push_back(selected);

    if (selected == ItemId::Match) {
        return CreateItem(selected, 1 + (rng() % 3));
    }
    else if (selected == ItemId::ShotgunShells) {
        return CreateItem(selected, 2 + (rng() % 3));
    }
    return CreateItem(selected);
}

ItemId ItemManager::RollDeskItem(unsigned int seed) {
    std::mt19937 rng(seed);
    int roll = rng() % 100;

    if (roll < 2)  return ItemId::Shotgun;
    if (roll < 6)  return ItemId::OilBottle;
    if (roll < 26) return ItemId::OldDiary;
    if (roll < 36) return ItemId::GunParts;
    return ItemId::None;
}

bool ItemManager::UseItem(InventoryItem& item, Player& player, float& lanternOil, std::string& outMsg) {
    if (item.id == ItemId::OilBottle) {
        if (lanternOil >= 100.0f) {
            outMsg = "ランタンの油は満タンだ ";
            return false;
        }
        lanternOil = std::min(100.0f, lanternOil + 35.0f);
        outMsg = "油を補給した ";
        return true;
    }
    else if (item.id == ItemId::Bandage) {
        player.Heal(25);
        outMsg = "包帯を巻いて傷口を処置した ";
        return true;
    }
    else if (item.id == ItemId::Hemostatic) {
        player.Heal(50);
        outMsg = "止血剤を傷口に流し込んだ ";
        return true;
    }
    else if (item.id == ItemId::Painkiller) {
        player.Heal(35);
        outMsg = "鎮痛薬を飲み下し、痛みを麻痺させた ";
        return true;
    }
    else if (item.id == ItemId::GunParts) {
        outMsg = "作業台の上で使用してください ";
        return false;
    }
    return false;
}