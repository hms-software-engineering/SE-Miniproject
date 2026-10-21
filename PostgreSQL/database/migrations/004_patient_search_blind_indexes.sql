-- HMS-F-013 | Keyed indexes for patient ID, name-prefix, and phone search.
-- The C++ data-access layer computes HMAC-SHA-256 values. Search keys must be
-- supplied outside PostgreSQL and stored separately from the database.
BEGIN;

ALTER TABLE patients
    ADD COLUMN IF NOT EXISTS name_prefix_blind_indexes BYTEA[]
        NOT NULL DEFAULT ARRAY[]::BYTEA[],
    ADD COLUMN IF NOT EXISTS phone_blind_index BYTEA;

DROP INDEX IF EXISTS idx_patients_name_ciphertext;
DROP INDEX IF EXISTS idx_patients_phone_ciphertext;

CREATE INDEX IF NOT EXISTS idx_patients_name_prefix_blind
    ON patients USING GIN (name_prefix_blind_indexes);
CREATE INDEX IF NOT EXISTS idx_patients_phone_blind
    ON patients (phone_blind_index);

COMMIT;
