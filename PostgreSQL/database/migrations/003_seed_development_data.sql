-- HMS-T-02 | Development-only seed records.
-- These sentinel password_hash values are deliberately NOT valid hashes and
-- cannot be used to log in. Replace them with real Argon2id hashes generated
-- by the application before any authentication demo or deployment.
BEGIN;

INSERT INTO users (user_id, username, password_hash, role) VALUES
 (1, 'seed_receptionist', 'DISABLED_DEV_SEED_REPLACE_WITH_ARGON2ID_HASH', 'Receptionist'),
 (2, 'seed_doctor',       'DISABLED_DEV_SEED_REPLACE_WITH_ARGON2ID_HASH', 'Doctor'),
 (3, 'seed_patient',      'DISABLED_DEV_SEED_REPLACE_WITH_ARGON2ID_HASH', 'Patient'),
 (4, 'seed_billing',      'DISABLED_DEV_SEED_REPLACE_WITH_ARGON2ID_HASH', 'Billing Officer'),
 (5, 'seed_admin',        'DISABLED_DEV_SEED_REPLACE_WITH_ARGON2ID_HASH', 'Administrator')
ON CONFLICT (username) DO NOTHING;

-- Patient PHI columns are BYTEA ciphertext fields. These seeds use visibly
-- labelled demo placeholders, not real patient information or production ciphertext.
INSERT INTO patients (patient_id, user_id, name, dob, gender, phone, address, allergies)
SELECT 'PID-00000001', u.user_id, convert_to('DEMO_CIPHERTEXT_NAME_0001','UTF8'),
       convert_to('DEMO_CIPHERTEXT_DOB_0001','UTF8'), 'Not specified',
       convert_to('DEMO_CIPHERTEXT_PHONE_0001','UTF8'),
       convert_to('DEMO_CIPHERTEXT_ADDRESS_0001','UTF8'),
       convert_to('DEMO_CIPHERTEXT_ALLERGIES_NONE','UTF8')
FROM users u WHERE u.username = 'seed_patient'
ON CONFLICT (patient_id) DO NOTHING;

INSERT INTO doctors (doctor_id, user_id, specialty, fee, working_hours)
SELECT 1, u.user_id, 'General Medicine', 500.00,
       '{"monday":"09:00-13:00","wednesday":"09:00-13:00","friday":"14:00-17:00"}'::jsonb
FROM users u WHERE u.username = 'seed_doctor'
ON CONFLICT (user_id) DO NOTHING;

-- A second demo patient is not linked to a login account, for search/list testing.
INSERT INTO patients (patient_id, name, dob, gender, phone, address, allergies)
VALUES ('PID-00000002', convert_to('DEMO_CIPHERTEXT_NAME_0002','UTF8'),
        convert_to('DEMO_CIPHERTEXT_DOB_0002','UTF8'), 'Not specified',
        convert_to('DEMO_CIPHERTEXT_PHONE_0002','UTF8'),
        convert_to('DEMO_CIPHERTEXT_ADDRESS_0002','UTF8'),
        convert_to('DEMO_CIPHERTEXT_ALLERGIES_UNKNOWN','UTF8'))
ON CONFLICT (patient_id) DO NOTHING;

SELECT setval(pg_get_serial_sequence('users','user_id'), GREATEST((SELECT MAX(user_id) FROM users),1), true);
SELECT setval(pg_get_serial_sequence('doctors','doctor_id'), GREATEST((SELECT MAX(doctor_id) FROM doctors),1), true);
COMMIT;
