#include "SimCharAnimCustomModelPlugin.h"
#include "ICharacterController.h"
#include <cstring>
#include <new>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// --- QUATERNION HELPER MATH ---
static arkheon_quat quat_identity() {
    return { 0.0f, 0.0f, 0.0f, 1.0f };
}

static arkheon_quat quat_from_axis_angle(float ax, float ay, float az, float angle_rad) {
    float half = angle_rad * 0.5f;
    float s = std::sin(half);
    float c = std::cos(half);
    return { ax * s, ay * s, az * s, c };
}

static arkheon_quat quat_normalize(arkheon_quat q) {
    float n2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (n2 <= 0.000001f) {
        return quat_identity();
    }
    float inv = 1.0f / std::sqrt(n2);
    return { q.x * inv, q.y * inv, q.z * inv, q.w * inv };
}

namespace {
    // WASD Tuş İndeksleri
    constexpr int HID_W = 26;
    constexpr int HID_A = 4;
    constexpr int HID_S = 22;
    constexpr int HID_D = 7;

    // Mod Seçim Tuş İndeksleri (Klavye Üst Sıra: 1, 2, 3)
    constexpr int HID_KEY_1 = 30;
    constexpr int HID_KEY_2 = 31;
    constexpr int HID_KEY_3 = 32;

    enum MotionMode {
        MODE_WALK = 0,
        MODE_PUSH = 1,
        MODE_CLIMB = 2
    };

    struct Controller {
        float seg_len[10] = { 0 };
        arkheon_quat joint_pose[ARK_JOINT_COUNT] = {};
        
        // Kinematik Motor Değişkenleri
        float simulationTime = 0.0f;
        float walkSpeed = 5.0f;
        float timer = 0.0f;
        float fps = 24.0f; 
        
        // Aktif Animasyon Modu (Varsayılan: Walk)
        MotionMode currentMode = MODE_WALK;
    };
}

extern "C" {

    ARKHEON_CHAR_EXPORT uint32_t arkheon_character_sdk_version(void) {
        return ARKHEON_CHARACTER_SDK_VERSION;
    }

    // Arayüzün tanıması için isim doğrudan "AnimationModelNathanHuman" olarak kilitlendi
    ARKHEON_CHAR_EXPORT const char* arkheon_character_plugin_name(void) {
        return "AnimationModelNathanHuman";
    }

    ARKHEON_CHAR_EXPORT void arkheon_character_get_motion_clips(void* /*handle*/, int32_t out_clip_ids[3]) {
        out_clip_ids[0] = 12;  
        out_clip_ids[1] = 47;  
        out_clip_ids[2] = 83;  
    }

    ARKHEON_CHAR_EXPORT void* arkheon_character_create(const float segment_lengths_m[10]) {
        Controller* c = new (std::nothrow) Controller();
        if (!c) return nullptr;

        if (segment_lengths_m) {
            std::memcpy(c->seg_len, segment_lengths_m, sizeof(c->seg_len));
        }

        for (int i = 0; i < ARK_JOINT_COUNT; ++i) {
            c->joint_pose[i] = quat_identity();
        }
        return c;
    }

    ARKHEON_CHAR_EXPORT void arkheon_character_destroy(void* handle) {
        Controller* c = static_cast<Controller*>(handle);
        delete c;
    }

    ARKHEON_CHAR_EXPORT int32_t arkheon_character_tick(
        void* handle,
        const arkheon_frame* frame,
        const arkheon_bone_state /*in_bones*/[66],
        arkheon_bone_override out_overrides[10],
        arkheon_vec3* out_root_translation_delta,
        arkheon_quat* out_root_rotation_delta,
        const arkheon_input_state* input,
        const arkheon_mission_goal* current_goal,
        const arkheon_env_api* env)
    {
        if (!handle || !out_overrides || !out_root_translation_delta || !out_root_rotation_delta) return 1;

        Controller* c = static_cast<Controller*>(handle);
        float dt = (frame && frame->delta_time_s > 0.0) ? static_cast<float>(frame->delta_time_s) : 0.02f;

        // 1. Runtime Dinamik Mod Geçişi Kontrolü
        if (input) {
            if (input->keys[HID_KEY_1]) c->currentMode = MODE_WALK;
            if (input->keys[HID_KEY_2]) c->currentMode = MODE_PUSH;
            if (input->keys[HID_KEY_3]) c->currentMode = MODE_CLIMB;
        }

        // 2. Kök Hareket Hesaplaması (WASD)
        float move_x = 0.0f;
        float move_z = 0.0f;

        if (input) {
            if (input->keys[HID_W]) move_z += 1.0f;
            if (input->keys[HID_S]) move_z -= 1.0f;
            if (input->keys[HID_D]) move_x += 1.0f;
            if (input->keys[HID_A]) move_x -= 1.0f;
        }

        float len = std::sqrt(move_x * move_x + move_z * move_z);
        bool isMoving = len > 0.0001f;
        
        if (isMoving) {
            move_x /= len;
            move_z /= len;
        }

        float yaw = input ? input->look_yaw_rad : 0.0f;
        float sin_yaw = std::sin(yaw);
        float cos_yaw = std::cos(yaw);
        float world_x = move_x * cos_yaw + move_z * sin_yaw;
        float world_z = -move_x * sin_yaw + move_z * cos_yaw;

        float speed = 1.4f;
        arkheon_vec3 root_delta = { 0.0f, 0.0f, 0.0f };
        
        if (c->currentMode == MODE_CLIMB) {
            if (input && input->keys[HID_W]) root_delta.y = speed * dt;
            if (input && input->keys[HID_S]) root_delta.y = -speed * dt;
        } else {
            root_delta.x = world_x * speed * dt;
            root_delta.z = world_z * speed * dt;
        }

        // 3. 24 FPS Kinematik Hesaplama Motoru
        c->timer += dt;
        arkheon_quat target[ARK_JOINT_COUNT];
        for (int i = 0; i < ARK_JOINT_COUNT; ++i) target[i] = quat_identity();

        if (c->timer >= (1.0f / c->fps)) {
            c->timer = 0.0f;
            
            bool activeMovement = (c->currentMode == MODE_CLIMB) ? 
                (input && (input->keys[HID_W] || input->keys[HID_S])) : isMoving;

            if (activeMovement) {
                c->simulationTime += (1.0f / c->fps);
            } else {
                c->simulationTime = 0.0f; 
            }
            
            float cycleSin = std::sin(c->simulationTime * c->walkSpeed);
            float cycleCos = std::cos(c->simulationTime * c->walkSpeed);
            float deg2rad = M_PI / 180.0f;

            // --- MOD 1: WALK (YÜRÜME) ---
            if (c->currentMode == MODE_WALK) {
                if (activeMovement) {
                    target[ARK_JOINT_THIGH_L] = quat_from_axis_angle(1, 0, 0, cycleSin * 30.0f * deg2rad);
                    target[ARK_JOINT_THIGH_R] = quat_from_axis_angle(1, 0, 0, -cycleSin * 30.0f * deg2rad);

                    float leftKnee = (cycleSin > 0) ? -5.0f : -45.0f * std::abs(cycleSin);
                    float rightKnee = (cycleSin < 0) ? -5.0f : -45.0f * std::abs(cycleSin);
                    target[ARK_JOINT_CALF_L] = quat_from_axis_angle(1, 0, 0, leftKnee * deg2rad);
                    target[ARK_JOINT_CALF_R] = quat_from_axis_angle(1, 0, 0, rightKnee * deg2rad);

                    target[ARK_JOINT_UPPERARM_L] = quat_from_axis_angle(1, 0, 0, -cycleSin * 25.0f * deg2rad);
                    target[ARK_JOINT_UPPERARM_R] = quat_from_axis_angle(1, 0, 0, cycleSin * 25.0f * deg2rad);
                    target[ARK_JOINT_LOWERARM_L] = quat_from_axis_angle(1, 0, 0, 20.0f * deg2rad);
                    target[ARK_JOINT_LOWERARM_R] = quat_from_axis_angle(1, 0, 0, 20.0f * deg2rad);
                } else {
                    target[ARK_JOINT_CALF_L] = quat_from_axis_angle(1, 0, 0, -5.0f * deg2rad);
                    target[ARK_JOINT_CALF_R] = quat_from_axis_angle(1, 0, 0, -5.0f * deg2rad);
                }
            }
            // --- MOD 2: PUSH (AĞIRLIK İTME) ---
            else if (c->currentMode == MODE_PUSH) {
                target[ARK_JOINT_UPPERARM_L] = quat_from_axis_angle(1, 0, 0, 45.0f * deg2rad); 
                target[ARK_JOINT_UPPERARM_R] = quat_from_axis_angle(1, 0, 0, 45.0f * deg2rad);
                target[ARK_JOINT_LOWERARM_L] = quat_from_axis_angle(1, 0, 0, 30.0f * deg2rad); 
                target[ARK_JOINT_LOWERARM_R] = quat_from_axis_angle(1, 0, 0, 30.0f * deg2rad);

                if (activeMovement) {
                    target[ARK_JOINT_THIGH_L] = quat_from_axis_angle(1, 0, 0, cycleSin * 15.0f * deg2rad);
                    target[ARK_JOINT_THIGH_R] = quat_from_axis_angle(1, 0, 0, -cycleSin * 15.0f * deg2rad);
                    target[ARK_JOINT_CALF_L] = quat_from_axis_angle(1, 0, 0, -15.0f * std::abs(cycleSin) * deg2rad);
                    target[ARK_JOINT_CALF_R] = quat_from_axis_angle(1, 0, 0, -15.0f * std::abs(-cycleSin) * deg2rad);
                }
            }
            // --- MOD 3: CLIMB (TIRMANMA) ---
            else if (c->currentMode == MODE_CLIMB) {
                if (activeMovement) {
                    target[ARK_JOINT_UPPERARM_L] = quat_from_axis_angle(1, 0, 0, (cycleSin * 35.0f + 60.0f) * deg2rad);
                    target[ARK_JOINT_UPPERARM_R] = quat_from_axis_angle(1, 0, 0, (-cycleSin * 35.0f + 60.0f) * deg2rad);
                    target[ARK_JOINT_LOWERARM_L] = quat_from_axis_angle(1, 0, 0, (std::abs(cycleSin) * 40.0f + 10.0f) * deg2rad);
                    target[ARK_JOINT_LOWERARM_R] = quat_from_axis_angle(1, 0, 0, (std::abs(-cycleSin) * 40.0f + 10.0f) * deg2rad);

                    target[ARK_JOINT_THIGH_L] = quat_from_axis_angle(1, 0, 0, cycleCos * 25.0f * deg2rad);
                    target[ARK_JOINT_THIGH_R] = quat_from_axis_angle(1, 0, 0, -cycleCos * 25.0f * deg2rad);
                    target[ARK_JOINT_CALF_L] = quat_from_axis_angle(1, 0, 0, -50.0f * std::abs(cycleCos) * deg2rad);
                    target[ARK_JOINT_CALF_R] = quat_from_axis_angle(1, 0, 0, -50.0f * std::abs(-cycleCos) * deg2rad);
                } else {
                    target[ARK_JOINT_UPPERARM_L] = quat_from_axis_angle(1, 0, 0, 60.0f * deg2rad);
                    target[ARK_JOINT_UPPERARM_R] = quat_from_axis_angle(1, 0, 0, 60.0f * deg2rad);
                    target[ARK_JOINT_THIGH_L] = quat_from_axis_angle(1, 0, 0, 15.0f * deg2rad);
                    target[ARK_JOINT_THIGH_R] = quat_from_axis_angle(1, 0, 0, 15.0f * deg2rad);
                }
            }

            // Smooth Interpolation
            float alpha = 0.25f;
            for (int i = 0; i < ARK_JOINT_COUNT; ++i) {
                c->joint_pose[i].x += (target[i].x - c->joint_pose[i].x) * alpha;
                c->joint_pose[i].y += (target[i].y - c->joint_pose[i].y) * alpha;
                c->joint_pose[i].z += (target[i].z - c->joint_pose[i].z) * alpha;
                c->joint_pose[i].w += (target[i].w - c->joint_pose[i].w) * alpha;
                c->joint_pose[i] = quat_normalize(c->joint_pose[i]);
            }
        }

        // 4. Verileri sisteme geri besleme
        for (int i = 0; i < ARK_JOINT_COUNT; ++i) {
            out_overrides[i].local_rotation = c->joint_pose[i];
            out_overrides[i].apply = 1;
        }

        *out_root_translation_delta = root_delta;
        *out_root_rotation_delta = { 0.0f, 0.0f, 0.0f, 1.0f };

        return 0;
    }

} // extern "C"