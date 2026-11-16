INSERT INTO users (user_id, username, password_hash, role) VALUES
 (81000001, 'stage8_clinical_doctor', 'test-only-not-a-credential', 'Doctor'),
 (81000002, 'stage8_clinical_other_doctor', 'test-only-not-a-credential', 'Doctor'),
 (81000003, 'stage8_clinical_patient', 'test-only-not-a-credential', 'Patient');
INSERT INTO doctors (doctor_id, user_id, specialty, fee) VALUES
 (82000001, 81000001, 'Synthetic Test', 0),
 (82000002, 81000002, 'Synthetic Test', 0);
UPDATE doctors SET working_hours = '{"monday":"00:00-23:30","tuesday":"00:00-23:30","wednesday":"00:00-23:30","thursday":"00:00-23:30","friday":"00:00-23:30","saturday":"00:00-23:30","sunday":"00:00-23:30"}'::jsonb WHERE doctor_id = 82000001;
INSERT INTO patients (patient_id, user_id, name, dob, gender, phone, address) VALUES
 ('PID-00000001', 81000003, decode('01','hex'), decode('01','hex'), 'Unspecified', decode('01','hex'), decode('01','hex'));
INSERT INTO appointments (appointment_id, patient_id, doctor_id, slot_start, status) VALUES
 (83000001, 'PID-00000001', 82000001, date_trunc('hour', now()) + interval '1 day', 'Confirmed'),
 (83000002, 'PID-00000001', 82000001, date_trunc('hour', now()) + interval '1 day 30 minutes', 'Confirmed'),
 (83000003, 'PID-00000001', 82000001, date_trunc('hour', now()) + interval '2 days', 'Cancelled');
