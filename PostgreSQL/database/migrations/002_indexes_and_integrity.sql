-- HMS-T-02 | Search indexes and appointment integrity constraints
BEGIN;

-- SAD 3.7 explicitly calls for patient name and phone indexes. Those values are
-- encrypted at rest per SAD 4.1, so ordinary indexes cannot support plaintext
-- search. These indexes support ID lookup and encrypted-value equality only;
-- name/phone search requires a reviewed keyed blind-index design in the DAL.
CREATE INDEX IF NOT EXISTS idx_patients_patient_id ON patients(patient_id);
CREATE INDEX IF NOT EXISTS idx_patients_name_ciphertext ON patients(name);
CREATE INDEX IF NOT EXISTS idx_patients_phone_ciphertext ON patients(phone);
CREATE INDEX IF NOT EXISTS idx_appointments_doctor_slot ON appointments(doctor_id, slot_start);
CREATE INDEX IF NOT EXISTS idx_appointments_patient_slot ON appointments(patient_id, slot_start DESC);
CREATE INDEX IF NOT EXISTS idx_patient_history_patient_changed ON patient_history(patient_id, changed_at DESC);
CREATE INDEX IF NOT EXISTS idx_visits_doctor_time ON visits(doctor_id, visited_at DESC);
CREATE INDEX IF NOT EXISTS idx_prescriptions_visit ON prescriptions(visit_id);
CREATE INDEX IF NOT EXISTS idx_bill_items_bill ON bill_items(bill_id);
CREATE INDEX IF NOT EXISTS idx_payments_bill ON payments(bill_id);
CREATE INDEX IF NOT EXISTS idx_audit_log_timestamp ON audit_log("timestamp" DESC);

-- Only confirmed bookings reserve a doctor's slot. Cancelled/completed rows do not.
CREATE UNIQUE INDEX IF NOT EXISTS uq_appointments_confirmed_doctor_slot
    ON appointments(doctor_id, slot_start)
    WHERE status = 'Confirmed';

-- SAD specifies audit_log as insert-only. Prevent row mutation/deletion through SQL.
CREATE OR REPLACE FUNCTION reject_audit_log_mutation()
RETURNS TRIGGER LANGUAGE plpgsql AS $$
BEGIN
    RAISE EXCEPTION 'audit_log is insert-only';
END;
$$;

DROP TRIGGER IF EXISTS trg_audit_log_no_update_delete ON audit_log;
CREATE TRIGGER trg_audit_log_no_update_delete
BEFORE UPDATE OR DELETE ON audit_log
FOR EACH ROW EXECUTE FUNCTION reject_audit_log_mutation();

COMMIT;
