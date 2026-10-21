-- HMS-F-010 | Generate patient IDs safely across concurrent registrations.
BEGIN;

CREATE SEQUENCE IF NOT EXISTS patient_id_sequence AS BIGINT;

DO $$
DECLARE
    next_patient_id BIGINT;
BEGIN
    SELECT COALESCE(MAX(SUBSTRING(patient_id FROM 5)::BIGINT), 0) + 1
      INTO next_patient_id
      FROM patients;
    IF next_patient_id > 99999999 THEN
        RAISE EXCEPTION 'Patient ID range is exhausted';
    END IF;
    PERFORM setval('patient_id_sequence', next_patient_id, FALSE);
END $$;

COMMIT;
