#include "combat_use_cases.h"

namespace defn {

namespace {

void append_movement_commands(const CombatLogicIntent &intent, std::vector<CombatCommand> &commands) {
    if (intent.movement == CombatMovementIntent::STOP) {
        commands.push_back({.type = CombatCommandType::STOP});
    } else if (intent.movement == CombatMovementIntent::MOVE) {
        commands.push_back({.type = CombatCommandType::MOVE});
    }

    if (intent.pose != CombatPoseIntent::NONE) {
        commands.push_back({.type = CombatCommandType::PLAY_POSE, .pose = intent.pose});
    }

    if (intent.hide_muzzle_flash) {
        commands.push_back({.type = CombatCommandType::HIDE_MUZZLE_FLASH});
    }
}

void append_damage_commands(EntityId target_id, int damage, Color color, std::vector<CombatCommand> &commands) {
    commands.push_back({.type = CombatCommandType::DEAL_DAMAGE, .target_id = target_id, .damage = damage});
    commands.push_back({.type = CombatCommandType::PLAY_EFFECT, .effect = CombatEffectType::DAMAGE_FLASH, .target_id = target_id, .color = color});
}

void append_attack_commands(const CombatConfig &config, const CombatTargetSelection &selection, std::vector<CombatCommand> &commands) {
    if (!selection.target_id.is_valid()) {
        return;
    }

    if (selection.attack_mode == AttackMode::MELEE) {
        commands.push_back({.type = CombatCommandType::PLAY_EFFECT, .effect = CombatEffectType::MELEE_ATTACK, .target_id = selection.target_id});
        append_damage_commands(selection.target_id, config.melee_damage, config.melee_flash_color, commands);
        return;
    }

    if (selection.attack_mode != AttackMode::RANGED) {
        return;
    }

    if (config.projectile_attack.has_value()) {
        commands.push_back({
            .type = CombatCommandType::SPAWN_PROJECTILE,
            .target_id = selection.target_id,
            .target_position = selection.target_position,
            .damage = config.ranged_damage,
            .color = config.ranged_flash_color,
            .projectile = *config.projectile_attack,
        });
        commands.push_back({.type = CombatCommandType::PLAY_EFFECT, .effect = CombatEffectType::RANGED_SHOOT, .target_id = selection.target_id});
        return;
    }

    commands.push_back({.type = CombatCommandType::PLAY_EFFECT, .effect = CombatEffectType::RANGED_SHOOT, .target_id = selection.target_id});
    append_damage_commands(selection.target_id, config.ranged_damage, config.ranged_flash_color, commands);
}

} // namespace

AdvanceCombatOutput advance_combat(const CombatConfig &config, const CombatLogicInput &input) {
    const CombatLogicStep step = advance_combat_logic(config, input);
    AdvanceCombatOutput output;
    output.state = step.state;
    append_movement_commands(step.intent, output.commands);

    if (step.intent.trigger_attack) {
        append_attack_commands(config, input.selection, output.commands);
    }

    return output;
}

} // namespace defn