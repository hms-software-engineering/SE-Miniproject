\set ON_ERROR_STOP on

-- HMS-T-02 schema/integrity smoke tests. Run against a disposable development DB.
DO $$
DECLARE
  expected_tables TEXT[] := ARRAY[
    'users','auth_sessions','patients','patient_history','doctors','appointments','visits',
    'prescriptions','bills','bill_items','payments','audit_log'
  ];
  required_columns TEXT[] := ARRAY[
    'users.user_id','users.username','users.password_hash','users.role','users.active',
    'users.failed_attempts','users.locked_until',
    'auth_sessions.token_hash','auth_sessions.user_id','auth_sessions.expires_at',
    'patients.patient_id','patients.name','patients.dob','patients.gender','patients.phone',
    'patients.address','patients.allergies','patients.name_prefix_blind_indexes',
    'patients.phone_blind_index',
    'patient_history.patient_id','patient_history.old_values',
    'doctors.doctor_id','doctors.user_id','doctors.specialty','doctors.fee','doctors.working_hours',
    'appointments.appointment_id','appointments.patient_id','appointments.doctor_id',
    'appointments.slot_start','appointments.status',
    'visits.visit_id','visits.appointment_id','visits.symptoms','visits.diagnosis','visits.notes',
    'prescriptions.visit_id','prescriptions.item','prescriptions.dosage','prescriptions.frequency',
    'prescriptions.days','prescriptions.override_reason',
    'bills.bill_id','bills.visit_id','bills.subtotal','bills.tax','bills.total','bills.status',
    'bill_items.bill_id','bill_items.item','bill_items.amount',
    'payments.payment_id','payments.bill_id','payments.amount','payments.mode','payments.receipt_no',
    'audit_log.audit_id','audit_log.user_id','audit_log.role','audit_log.action',
    'audit_log.record_id','audit_log.timestamp','audit_log.source_ip'
  ];
  required_indexes TEXT[] := ARRAY[
    'idx_patients_patient_id','idx_patients_name_prefix_blind','idx_patients_phone_blind',
    'idx_auth_sessions_expiration',
    'idx_appointments_doctor_slot','idx_appointments_patient_slot',
    'idx_patient_history_patient_changed','idx_visits_doctor_time','idx_prescriptions_visit',
    'idx_bill_items_bill','idx_payments_bill','idx_audit_log_timestamp',
    'uq_appointments_confirmed_doctor_slot'
  ];
  item TEXT;
  v_table_name TEXT;
  v_column_name TEXT;
  table_item TEXT;
BEGIN
  FOREACH table_item IN ARRAY expected_tables LOOP
    IF to_regclass('public.' || table_item) IS NULL THEN
      RAISE EXCEPTION 'Missing required table: %', table_item;
    END IF;
  END LOOP;

  FOREACH item IN ARRAY required_columns LOOP
    v_table_name := split_part(item, '.', 1);
    v_column_name := split_part(item, '.', 2);
    IF NOT EXISTS (
      SELECT 1 FROM information_schema.columns c
      WHERE c.table_schema = 'public'
        AND c.table_name = v_table_name
        AND c.column_name = v_column_name
    ) THEN
      RAISE EXCEPTION 'Missing required column: %', item;
    END IF;
  END LOOP;

  FOREACH item IN ARRAY required_indexes LOOP
    IF NOT EXISTS (
      SELECT 1 FROM pg_indexes i
      WHERE i.schemaname = 'public' AND i.indexname = item
    ) THEN
      RAISE EXCEPTION 'Missing required index: %', item;
    END IF;
  END LOOP;

  IF (SELECT COUNT(*) FROM users WHERE username IN
      ('seed_receptionist','seed_doctor','seed_patient','seed_billing','seed_admin')) <> 5 THEN
    RAISE EXCEPTION 'Expected one seed user for each of the five roles';
  END IF;
  IF (SELECT COUNT(DISTINCT role) FROM users WHERE username IN
      ('seed_receptionist','seed_doctor','seed_patient','seed_billing','seed_admin')) <> 5 THEN
    RAISE EXCEPTION 'Seed users do not cover all five roles';
  END IF;
  IF (SELECT COUNT(*) FROM doctors) < 1 THEN RAISE EXCEPTION 'Missing seeded doctor'; END IF;
  IF (SELECT COUNT(*) FROM patients) < 2 THEN RAISE EXCEPTION 'Expected at least two sample patients'; END IF;
  IF to_regclass('public.patient_id_sequence') IS NULL THEN
    RAISE EXCEPTION 'Missing patient ID sequence';
  END IF;
  IF NOT EXISTS (
    SELECT 1 FROM pg_trigger
    WHERE tgname = 'trg_audit_log_no_update_delete' AND NOT tgisinternal
  ) THEN
    RAISE EXCEPTION 'Missing insert-only audit trigger';
  END IF;
END $$;

-- Prove duplicate confirmed booking is rejected; roll back the probe afterwards.
BEGIN;
INSERT INTO appointments(patient_id, doctor_id, slot_start, status)
VALUES ('PID-00000001', 1, TIMESTAMPTZ '2030-01-01 09:00:00+00', 'Confirmed');
DO $$ BEGIN
  BEGIN
    INSERT INTO appointments(patient_id, doctor_id, slot_start, status)
    VALUES ('PID-00000002', 1, TIMESTAMPTZ '2030-01-01 09:00:00+00', 'Confirmed');
    RAISE EXCEPTION 'Duplicate confirmed appointment was unexpectedly accepted';
  EXCEPTION WHEN unique_violation THEN
    RAISE NOTICE 'PASS: duplicate confirmed appointment rejected';
  END;
END $$;
ROLLBACK;

-- Prove audit rows cannot be updated or deleted.
BEGIN;
INSERT INTO audit_log(role, action, record_id) VALUES ('Administrator','T02_TEST','test-row');
DO $$ DECLARE aid BIGINT; BEGIN
  SELECT audit_id INTO aid FROM audit_log
  WHERE action='T02_TEST' AND record_id='test-row'
  ORDER BY audit_id DESC LIMIT 1;
  BEGIN
    UPDATE audit_log SET action='tampered' WHERE audit_id=aid;
    RAISE EXCEPTION 'Audit update was unexpectedly accepted';
  EXCEPTION WHEN raise_exception THEN
    IF SQLERRM <> 'audit_log is insert-only' THEN RAISE; END IF;
    RAISE NOTICE 'PASS: audit update rejected';
  END;
  BEGIN
    DELETE FROM audit_log WHERE audit_id=aid;
    RAISE EXCEPTION 'Audit delete was unexpectedly accepted';
  EXCEPTION WHEN raise_exception THEN
    IF SQLERRM <> 'audit_log is insert-only' THEN RAISE; END IF;
    RAISE NOTICE 'PASS: audit delete rejected';
  END;
END $$;
ROLLBACK;

SELECT 'PASS: required tables, key columns, indexes, seed data, appointment uniqueness and audit immutability verified' AS result;
