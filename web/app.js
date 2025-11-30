// Configuration
const API_BASE = 'http://localhost:8080';

// State
let state = {
    students: [],
    courses: [],
    enrollments: [],
    currentView: 'students',
    importType: null,
    pagination: {
        students: { page: 0, perPage: 10 },
        courses: { page: 0, perPage: 10 },
        enrollments: { page: 0, perPage: 10 }
    }
};

// API Client
const api = {
    async request(endpoint, options = {}) {
        try {
            const response = await fetch(`${API_BASE}${endpoint}`, {
                ...options,
                headers: {
                    'Content-Type': 'application/json',
                    ...options.headers
                }
            });
            
            if (!response.ok) {
                const errorData = await response.json().catch(() => ({}));
                throw new Error(errorData.error || `HTTP ${response.status}`);
            }
            
            return await response.json();
        } catch (error) {
            console.error('API Error:', error);
            throw error;
        }
    },

    // Students
    async getStudents() {
        return await this.request('/student');
    },

    async addStudent(student) {
        const body = {
            student_id: student.student_id,
            name: student.name || "",
            email: student.email || "",
            credits: parseFloat(student.credits) || 0
        };
        
        return await this.request('/student/add', { 
            method: 'POST',
            body: JSON.stringify(body)
        });
    },

    async removeStudent(studentId) {
        return await this.request(`/student?student_id=${encodeURIComponent(studentId)}`, { method: 'DELETE' });
    },

    async updateStudent(student) {
        const body = {
            student_id: student.student_id,
            name: student.name || "",
            email: student.email || "",
            credits: parseFloat(student.credits) || 0
        };
        
        return await this.request('/student/update', { 
            method: 'PUT',
            body: JSON.stringify(body)
        });
    },

    async findStudentById(studentId) {
        return await this.request(`/student/find?student_id=${encodeURIComponent(studentId)}`);
    },

    async searchStudents(name) {
        return await this.request(`/student/find?name=${encodeURIComponent(name)}`);
    },

    // Courses
    async getCourses() {
        return await this.request('/course');
    },

    async addCourse(course) {
        const body = {
            course_id: course.course_id,
            credit: parseFloat(course.credit) || 0
        };
        
        if (course.name) body.name = course.name;
        if (course.type) body.type = course.type;
        if (course.total_hours) body.total_hours = parseFloat(course.total_hours);
        if (course.lecture_hours) body.lecture_hours = parseFloat(course.lecture_hours);
        if (course.lab_hours) body.lab_hours = parseFloat(course.lab_hours);
        if (course.semester) body.semester = course.semester;
        
        return await this.request('/course/add', { 
            method: 'POST',
            body: JSON.stringify(body)
        });
    },

    async removeCourse(courseId) {
        return await this.request(`/course?course_id=${encodeURIComponent(courseId)}`, { method: 'DELETE' });
    },

    async updateCourse(course) {
        const body = {
            course_id: course.course_id,
            credit: parseFloat(course.credit) || 0
        };
        
        if (course.name) body.name = course.name;
        if (course.type) body.type = course.type;
        if (course.total_hours) body.total_hours = parseFloat(course.total_hours);
        if (course.lecture_hours) body.lecture_hours = parseFloat(course.lecture_hours);
        if (course.lab_hours) body.lab_hours = parseFloat(course.lab_hours);
        if (course.semester) body.semester = course.semester;
        
        return await this.request('/course/update', { 
            method: 'PUT',
            body: JSON.stringify(body)
        });
    },

    async findCoursesByType(type) {
        return await this.request(`/course/find?type=${encodeURIComponent(type)}`);
    },

    async findCoursesBySemester(semester) {
        return await this.request(`/course/find?semester=${encodeURIComponent(semester)}`);
    },

    // Enrollments
    async getEnrollments() {
        return await this.request('/enrollment');
    },

    async getEnrollmentsByCourse(courseId) {
        return await this.request(`/enrollment?course_id=${encodeURIComponent(courseId)}`);
    },

    async getEnrollmentsByStudent(studentId) {
        return await this.request(`/enrollment?student_id=${encodeURIComponent(studentId)}`);
    },

    async addEnrollment(enrollment) {
        const body = {
            student_id: enrollment.student_id,
            course_id: enrollment.course_id
        };
        
        return await this.request('/enrollment', { 
            method: 'POST',
            body: JSON.stringify(body)
        });
    },

    async removeEnrollment(studentId, courseId) {
        return await this.request(
            `/enrollment?student_id=${encodeURIComponent(studentId)}&course_id=${encodeURIComponent(courseId)}`, 
            { method: 'DELETE' }
        );
    }
    ,
    async deleteAllStudents() {
        return await this.request('/student/all', { method: 'DELETE' });
    },
    async deleteAllCourses() {
        return await this.request('/course/all', { method: 'DELETE' });
    },
    async deleteAllEnrollments() {
        return await this.request('/enrollment/all', { method: 'DELETE' });
    }
};

// UI Functions
function showStatus(message, type = 'success') {
    const statusEl = document.createElement('div');
    statusEl.className = `status-message ${type}`;
    statusEl.textContent = message;
    document.body.appendChild(statusEl);
    
    setTimeout(() => {
        statusEl.remove();
    }, 3000);
}

function showModal(modalId) {
    document.getElementById(modalId).classList.add('show');
}

function closeModal(modalId) {
    document.getElementById(modalId).classList.remove('show');
}

// Navigation
document.querySelectorAll('.nav-btn').forEach(btn => {
    btn.addEventListener('click', () => {
        const view = btn.dataset.view;
        
        // Update navigation
        document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        
        // Update views
        document.querySelectorAll('.view-section').forEach(section => section.classList.remove('active'));
        document.getElementById(`${view}-view`).classList.add('active');
        
        state.currentView = view;
        
        // Load data for the view
        if (view === 'students') loadStudents();
        else if (view === 'courses') loadCourses();
        else if (view === 'enrollments') loadEnrollments();
        // student-courses view loads on demand
    });
});

// Students Management
async function loadStudents() {
    const tbody = document.getElementById('students-tbody');
    tbody.innerHTML = '<tr><td colspan="5" class="loading">加载中...</td></tr>';
    
    try {
        const data = await api.getStudents();
        // API returns an array directly
        state.students = Array.isArray(data) ? data : [];
        state.pagination.students.page = 0; // Reset to first page
        renderStudents();
    } catch (error) {
        tbody.innerHTML = `<tr><td colspan="5" class="no-data">错误: ${error.message}</td></tr>`;
    }
}

function renderStudents(students = state.students) {
    const tbody = document.getElementById('students-tbody');
    
    if (!students || students.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" class="no-data">未找到学生</td></tr>';
        updatePageInfo('students', []);
        return;
    }
    
    const { page, perPage } = state.pagination.students;
    const start = page * perPage;
    const end = start + perPage;
    const pageData = students.slice(start, end);
    
    tbody.innerHTML = pageData.map(student => `
        <tr>
            <td>${escapeHtml(student.student_id)}</td>
            <td>${escapeHtml(student.name || '-')}</td>
            <td>${escapeHtml(student.email || '-')}</td>
            <td>${student.credits || 0}</td>
            <td>
                <button class="btn btn-primary btn-small" onclick='showEditStudentModal(${JSON.stringify(student)})'>编辑</button>
                <button class="btn btn-danger btn-small" onclick="deleteStudent('${escapeHtml(student.student_id)}')">删除</button>
            </td>
        </tr>
    `).join('');
    
    updatePageInfo('students', students);
}

function searchStudents() {
    const query = document.getElementById('student-search').value.toLowerCase();
    const filtered = state.students.filter(s => 
        (s.student_id && s.student_id.toLowerCase().includes(query)) ||
        (s.name && s.name.toLowerCase().includes(query)) ||
        (s.email && s.email.toLowerCase().includes(query))
    );
    renderStudents(filtered);
}

function showAddStudentModal() {
    document.getElementById('add-student-form').reset();
    showModal('add-student-modal');
}

async function addStudent(event) {
    event.preventDefault();
    
    const student = {
        student_id: document.getElementById('student-id').value,
        name: document.getElementById('student-name').value,
        email: document.getElementById('student-email').value,
        credits: 0  // Always 0, will be auto-calculated from enrollments
    };
    
    try {
        await api.addStudent(student);
        showStatus('学生添加成功');
        closeModal('add-student-modal');
        loadStudents();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

async function deleteStudent(studentId) {
    if (!confirm(`删除学生 ${studentId}?`)) return;
    
    try {
        await api.removeStudent(studentId);
        showStatus('学生删除成功');
        loadStudents();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

async function deleteAllStudents() {
    if (!confirm('确定删除所有学生和其相关选课记录吗？此操作不可逆。')) return;
    try {
        await api.deleteAllStudents();
        showStatus('已删除所有学生');
        loadStudents();
        loadEnrollments();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

function showEditStudentModal(student) {
    document.getElementById('edit-student-id-hidden').value = student.student_id;
    document.getElementById('edit-student-id').value = student.student_id;
    document.getElementById('edit-student-name').value = student.name || '';
    document.getElementById('edit-student-email').value = student.email || '';
    showModal('edit-student-modal');
}

async function editStudent(event) {
    event.preventDefault();
    
    const student = {
        student_id: document.getElementById('edit-student-id-hidden').value,
        name: document.getElementById('edit-student-name').value,
        email: document.getElementById('edit-student-email').value,
        credits: 0  // Credits are auto-calculated
    };
    
    try {
        await api.updateStudent(student);
        showStatus('学生修改成功');
        closeModal('edit-student-modal');
        loadStudents();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

async function deleteAllEnrollments() {
    if (!confirm('确定删除所有选课记录吗？此操作不可逆。')) return;
    try {
        await api.deleteAllEnrollments();
        showStatus('已删除所有选课记录');
        loadEnrollments();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

async function deleteAllCourses() {
    if (!confirm('确定删除所有课程和其相关选课记录吗？此操作不可逆。')) return;
    try {
        await api.deleteAllCourses();
        showStatus('已删除所有课程');
        loadCourses();
        loadEnrollments();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

// Courses Management
async function loadCourses() {
    const tbody = document.getElementById('courses-tbody');
    tbody.innerHTML = '<tr><td colspan="9" class="loading">加载中...</td></tr>';
    
    try {
        const data = await api.getCourses();
        // API returns an array directly
        state.courses = Array.isArray(data) ? data : [];
        state.pagination.courses.page = 0; // Reset to first page
        renderCourses();
    } catch (error) {
        tbody.innerHTML = `<tr><td colspan="9" class="no-data">错误: ${error.message}</td></tr>`;
    }
}

function renderCourses(courses = state.courses) {

    function formatType(type) {
        switch (type) {
            case 'required': return '必修';
            case 'elective': return '选修';
            case 'general': return '通识';
            default: return type;
        }
    }

    function formatSemester(semester) {
        switch (semester) {
            case 'fall': return '秋季';
            case 'spring': return '春季';
            case 'summer': return '夏季';
            default: return semester;
        }
    }

    const tbody = document.getElementById('courses-tbody');
    
    if (!courses || courses.length === 0) {
        tbody.innerHTML = '<tr><td colspan="9" class="no-data">未找到课程</td></tr>';
        updatePageInfo('courses', []);
        return;
    }
    
    const { page, perPage } = state.pagination.courses;
    const start = page * perPage;
    const end = start + perPage;
    const pageData = courses.slice(start, end);
    
    tbody.innerHTML = pageData.map(course => `
        <tr>
            <td>${escapeHtml(course.course_id)}</td>
            <td>${escapeHtml(course.name || '-')}</td>
            <td>${escapeHtml(formatType(course.type) || '-')}</td>
            <td>${course.credit || 0}</td>
            <td>${course.total_hours || '-'}</td>
            <td>${course.lecture_hours || '-'}</td>
            <td>${course.lab_hours || '-'}</td>
            <td>${escapeHtml(formatSemester(course.semester) || '-')}</td>
            <td>
                <button class="btn btn-primary btn-small" onclick='showEditCourseModal(${JSON.stringify(course)})'>编辑</button>
                <button class="btn btn-danger btn-small" onclick="deleteCourse('${escapeHtml(course.course_id)}')">删除</button>
            </td>
        </tr>
    `).join('');
    
    updatePageInfo('courses', courses);
}

function searchCourses() {
    const query = document.getElementById('course-search').value.toLowerCase();
    const filtered = state.courses.filter(c => 
        (c.course_id && c.course_id.toLowerCase().includes(query)) ||
        (c.name && c.name.toLowerCase().includes(query))
    );
    renderCourses(filtered);
}

function filterCourses() {
    const typeFilter = document.getElementById('course-type-filter').value;
    const semesterFilter = document.getElementById('course-semester-filter').value;
    
    let filtered = state.courses;
    
    if (typeFilter) {
        filtered = filtered.filter(c => c.type === typeFilter);
    }
    
    if (semesterFilter) {
        filtered = filtered.filter(c => c.semester === semesterFilter);
    }
    
    renderCourses(filtered);
}

function showAddCourseModal() {
    document.getElementById('add-course-form').reset();
    showModal('add-course-modal');
}

async function addCourse(event) {
    event.preventDefault();
    
    const course = {
        course_id: document.getElementById('course-id').value,
        name: document.getElementById('course-name').value,
        type: document.getElementById('course-type').value,
        credit: parseFloat(document.getElementById('course-credit').value) || 0,
        total_hours: document.getElementById('course-total-hours').value,
        lecture_hours: document.getElementById('course-lecture-hours').value,
        lab_hours: document.getElementById('course-lab-hours').value,
        semester: document.getElementById('course-semester').value
    };
    
    try {
        await api.addCourse(course);
        showStatus('课程添加成功');
        closeModal('add-course-modal');
        loadCourses();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

async function deleteCourse(courseId) {
    if (!confirm(`删除课程 ${courseId}?`)) return;
    
    try {
        await api.removeCourse(courseId);
        showStatus('课程删除成功');
        loadCourses();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

function showEditCourseModal(course) {
    document.getElementById('edit-course-id-hidden').value = course.course_id;
    document.getElementById('edit-course-id').value = course.course_id;
    document.getElementById('edit-course-name').value = course.name || '';
    document.getElementById('edit-course-type').value = course.type || '';
    document.getElementById('edit-course-credit').value = course.credit || 0;
    document.getElementById('edit-course-total-hours').value = course.total_hours || '';
    document.getElementById('edit-course-lecture-hours').value = course.lecture_hours || '';
    document.getElementById('edit-course-lab-hours').value = course.lab_hours || '';
    document.getElementById('edit-course-semester').value = course.semester || '';
    showModal('edit-course-modal');
}

async function editCourse(event) {
    event.preventDefault();
    
    const course = {
        course_id: document.getElementById('edit-course-id-hidden').value,
        name: document.getElementById('edit-course-name').value,
        type: document.getElementById('edit-course-type').value,
        credit: parseFloat(document.getElementById('edit-course-credit').value) || 0,
        total_hours: document.getElementById('edit-course-total-hours').value,
        lecture_hours: document.getElementById('edit-course-lecture-hours').value,
        lab_hours: document.getElementById('edit-course-lab-hours').value,
        semester: document.getElementById('edit-course-semester').value
    };
    
    try {
        await api.updateCourse(course);
        showStatus('课程修改成功');
        closeModal('edit-course-modal');
        loadCourses();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

// Enrollments Management
async function loadEnrollments() {
    const tbody = document.getElementById('enrollments-tbody');
    tbody.innerHTML = '<tr><td colspan="5" class="loading">加载中...</td></tr>';
    
    try {
        // Load students and courses for lookup
        const studentsData = await api.getStudents();
        state.students = Array.isArray(studentsData) ? studentsData : [];
        
        const coursesData = await api.getCourses();
        state.courses = Array.isArray(coursesData) ? coursesData : [];
        
        // Load all enrollments using the new list endpoint
        const enrollData = await api.getEnrollments();
        const enrollments = Array.isArray(enrollData) ? enrollData : [];
        
        state.enrollments = enrollments.map(enroll => {
            const student = state.students.find(s => s.student_id === enroll.student_id);
            const course = state.courses.find(c => c.course_id === enroll.course_id);
            return {
                student_id: enroll.student_id,
                course_id: enroll.course_id,
                student_name: student?.name,
                course_name: course?.name
            };
        });
        
        state.pagination.enrollments.page = 0; // Reset to first page
        renderEnrollments();
        updateDataLists();
    } catch (error) {
        tbody.innerHTML = `<tr><td colspan="5" class="no-data">错误: ${error.message}</td></tr>`;
    }
}

function renderEnrollments(enrollments = state.enrollments) {
    const tbody = document.getElementById('enrollments-tbody');
    
    if (!enrollments || enrollments.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" class="no-data">未找到选课记录</td></tr>';
        updatePageInfo('enrollments', []);
        return;
    }
    
    const { page, perPage } = state.pagination.enrollments;
    const start = page * perPage;
    const end = start + perPage;
    const pageData = enrollments.slice(start, end);
    
    tbody.innerHTML = pageData.map(enroll => `
        <tr>
            <td>${escapeHtml(enroll.student_id)}</td>
            <td>${escapeHtml(enroll.student_name || '-')}</td>
            <td>${escapeHtml(enroll.course_id)}</td>
            <td>${escapeHtml(enroll.course_name || '-')}</td>
            <td>
                <button class="btn btn-primary btn-small" onclick='showEditEnrollmentModal(${JSON.stringify(enroll)})'>编辑</button>
                <button class="btn btn-danger btn-small" onclick="deleteEnrollment('${escapeHtml(enroll.student_id)}', '${escapeHtml(enroll.course_id)}')">删除</button>
            </td>
        </tr>
    `).join('');
    
    updatePageInfo('enrollments', enrollments);
}

function filterEnrollments() {
    const studentFilter = document.getElementById('enrollment-student-filter').value.toLowerCase();
    const courseFilter = document.getElementById('enrollment-course-filter').value.toLowerCase();
    
    let filtered = state.enrollments;
    
    if (studentFilter) {
        filtered = filtered.filter(e => 
            e.student_id.toLowerCase().includes(studentFilter)
        );
    }
    
    if (courseFilter) {
        filtered = filtered.filter(e => 
            e.course_id.toLowerCase().includes(courseFilter)
        );
    }
    
    renderEnrollments(filtered);
}

function showAddEnrollmentModal() {
    document.getElementById('add-enrollment-form').reset();
    updateDataLists();
    showModal('add-enrollment-modal');
}

function updateDataLists() {
    // Update student datalist
    const studentsList = document.getElementById('students-datalist');
    studentsList.innerHTML = state.students.map(s => 
        `<option value="${escapeHtml(s.student_id)}">${escapeHtml(s.name || s.student_id)}</option>`
    ).join('');
    
    // Update course datalist
    const coursesList = document.getElementById('courses-datalist');
    coursesList.innerHTML = state.courses.map(c => 
        `<option value="${escapeHtml(c.course_id)}">${escapeHtml(c.name || c.course_id)}</option>`
    ).join('');
}

async function addEnrollment(event) {
    event.preventDefault();
    
    const enrollment = {
        student_id: document.getElementById('enrollment-student-id').value,
        course_id: document.getElementById('enrollment-course-id').value
    };
    
    try {
        await api.addEnrollment(enrollment);
        showStatus('选课添加成功');
        closeModal('add-enrollment-modal');
        loadEnrollments();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

async function deleteEnrollment(studentId, courseId) {
    if (!confirm(`删除选课记录: ${studentId} - ${courseId}?`)) return;
    
    try {
        await api.removeEnrollment(studentId, courseId);
        showStatus('选课记录删除成功');
        loadEnrollments();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

function showEditEnrollmentModal(enrollment) {
    document.getElementById('edit-enrollment-old-student').value = enrollment.student_id;
    document.getElementById('edit-enrollment-old-course').value = enrollment.course_id;
    document.getElementById('edit-enrollment-student-id').value = enrollment.student_id;
    document.getElementById('edit-enrollment-course-id').value = enrollment.course_id;
    updateDataLists();
    showModal('edit-enrollment-modal');
}

async function editEnrollment(event) {
    event.preventDefault();
    
    const oldStudentId = document.getElementById('edit-enrollment-old-student').value;
    const oldCourseId = document.getElementById('edit-enrollment-old-course').value;
    const newStudentId = document.getElementById('edit-enrollment-student-id').value;
    const newCourseId = document.getElementById('edit-enrollment-course-id').value;
    
    try {
        // Delete old enrollment and add new one
        await api.removeEnrollment(oldStudentId, oldCourseId);
        await api.addEnrollment({
            student_id: newStudentId,
            course_id: newCourseId
        });
        showStatus('选课记录修改成功');
        closeModal('edit-enrollment-modal');
        loadEnrollments();
    } catch (error) {
        showStatus(`错误: ${error.message}`, 'error');
    }
}

// Student Courses Search
async function searchStudentCourses() {

    function formatType(type) {
        switch (type) {
            case 'required': return '必修';
            case 'elective': return '选修';
            case 'general': return '通识';
            default: return type;
        }
    }

    function formatSemester(semester) {
        switch (semester) {
            case 'fall': return '秋季';
            case 'spring': return '春季';
            case 'summer': return '夏季';
            default: return semester;
        }
    }

    const studentId = document.getElementById('student-courses-search').value.trim();
    const resultDiv = document.getElementById('student-courses-result');
    
    if (!studentId) {
        resultDiv.innerHTML = '<p class="info-text">请输入学号查询该学生的选课情况</p>';
        return;
    }
    
    resultDiv.innerHTML = '<p class="loading">加载中...</p>';
    
    try {
        // Get student info
        const students = await api.findStudentById(studentId);
        if (!students || students.length === 0) {
            resultDiv.innerHTML = '<p class="error-text">未找到该学生</p>';
            return;
        }
        
        const student = students[0];
        
        // Get enrollments
        const enrollments = await api.getEnrollmentsByStudent(studentId);
        
        // Get course details
        const allCourses = await api.getCourses();
        const enrolledCourses = enrollments.map(e => {
            const course = allCourses.find(c => c.course_id === e.course_id);
            return course;
        }).filter(c => c != null);
        
        // Render result
        let html = `
            <div class="student-info-card">
                <h3>学生信息</h3>
                <div class="info-row"><span class="label">学号:</span> <span class="value">${escapeHtml(student.student_id)}</span></div>
                <div class="info-row"><span class="label">姓名:</span> <span class="value">${escapeHtml(student.name || '-')}</span></div>
                <div class="info-row"><span class="label">邮箱:</span> <span class="value">${escapeHtml(student.email || '-')}</span></div>
                <div class="info-row"><span class="label">总学分:</span> <span class="value highlight">${student.credits || 0}</span></div>
                <div class="info-row"><span class="label">已选课程数:</span> <span class="value highlight">${enrolledCourses.length}</span></div>
            </div>
        `;
        
        if (enrolledCourses.length > 0) {
            html += '<div class="courses-list"><h3>已选课程列表</h3><table class="courses-table">';
            html += '<thead><tr><th>课程编号</th><th>课程名称</th><th>学分</th><th>类型</th><th>学期</th></tr></thead><tbody>';
            enrolledCourses.forEach(course => {
                html += `
                    <tr>
                        <td>${escapeHtml(course.course_id)}</td>
                        <td>${escapeHtml(course.name || '-')}</td>
                        <td>${course.credit || 0}</td>
                        <td>${escapeHtml(formatType(course.type) || '-')}</td>
                        <td>${escapeHtml(formatSemester(course.semester) || '-')}</td>
                    </tr>
                `;
            });
            html += '</tbody></table></div>';
        } else {
            html += '<p class="info-text">该学生尚未选课</p>';
        }
        
        resultDiv.innerHTML = html;
        
    } catch (error) {
        resultDiv.innerHTML = `<p class="error-text">错误: ${error.message}</p>`;
    }
}

// CSV Import
function showImportModal(type) {
    state.importType = type;
    document.getElementById('csv-file').value = '';
    document.getElementById('csv-preview').innerHTML = '';
    showModal('import-csv-modal');
}

document.getElementById('csv-file').addEventListener('change', async (event) => {
    const file = event.target.files[0];
    if (!file) return;
    
    try {
        const text = await file.text();
        const rows = parseCSV(text);
        
        if (rows.length === 0) {
            showStatus('空的CSV文件', 'error');
            return;
        }
        
        // Preview the CSV
        const preview = document.getElementById('csv-preview');
        preview.innerHTML = `
            <table>
                <thead>
                    <tr>${rows[0].map(cell => `<th>${escapeHtml(cell)}</th>`).join('')}</tr>
                </thead>
                <tbody>
                    ${rows.slice(1, 6).map(row => 
                        `<tr>${row.map(cell => `<td>${escapeHtml(cell)}</td>`).join('')}</tr>`
                    ).join('')}
                    ${rows.length > 6 ? '<tr><td colspan="100" style="text-align:center; font-style:italic;">... 及其他 ' + (rows.length - 6) + ' 行</td></tr>' : ''}
                </tbody>
            </table>
        `;
    } catch (error) {
        showStatus(`读取CSV错误: ${error.message}`, 'error');
    }
});

function parseCSV(text) {
    // Split by newlines but handle different line endings (\r\n, \n, \r)
    const lines = text.split(/\r?\n/).filter(line => line.trim());
    return lines.map(line => {
        const cells = [];
        let cell = '';
        let inQuotes = false;
        
        for (let i = 0; i < line.length; i++) {
            const char = line[i];
            
            if (char === '"') {
                inQuotes = !inQuotes;
            } else if (char === ',' && !inQuotes) {
                cells.push(cell.trim());
                cell = '';
            } else if (char !== '\r') {  // Skip carriage return characters
                cell += char;
            }
        }
        cells.push(cell.trim());
        
        return cells;
    });
}

async function importCSV() {
    const file = document.getElementById('csv-file').files[0];
    if (!file) {
        showStatus('请选择一个CSV文件', 'error');
        return;
    }
    
    try {
        const text = await file.text();
        const rows = parseCSV(text);
        
        if (rows.length < 2) {
            showStatus('CSV必须包含表头和至少一行数据', 'error');
            return;
        }
        
        const headers = rows[0].map(h => h.toLowerCase().trim());
        const dataRows = rows.slice(1);
        
        let successCount = 0;
        let errorCount = 0;
        
        // Process rows sequentially to avoid overwhelming the server
        for (let i = 0; i < dataRows.length; i++) {
            const row = dataRows[i];
            try {
                const obj = {};
                headers.forEach((header, idx) => {
                    if (row[idx]) {
                        obj[header] = row[idx].trim();
                    }
                });
                
                if (state.importType === 'students') {
                    await api.addStudent({
                        student_id: obj.student_id || obj.id,
                        name: obj.name,
                        email: obj.email,
                        credits: 0  // Always start at 0
                    });
                } else if (state.importType === 'courses') {
                    await api.addCourse({
                        course_id: obj.course_id || obj.id,
                        name: obj.name,
                        type: obj.type,
                        credit: obj.credit || obj.credits,
                        total_hours: obj.total_hours,
                        lecture_hours: obj.lecture_hours,
                        lab_hours: obj.lab_hours,
                        semester: obj.semester
                    });
                } else if (state.importType === 'enrollments') {
                    await api.addEnrollment({
                        student_id: obj.student_id,
                        course_id: obj.course_id
                    });
                }
                
                successCount++;
            } catch (error) {
                errorCount++;
                console.error('Import error for row:', row, error);
            }
        }
        
        showStatus(`导入完成: ${successCount} 成功, ${errorCount} 失败`);
        closeModal('import-csv-modal');
        
        // Reload data
        if (state.importType === 'students') loadStudents();
        else if (state.importType === 'courses') loadCourses();
        else if (state.importType === 'enrollments') loadEnrollments();
        
    } catch (error) {
        showStatus(`导入错误: ${error.message}`, 'error');
    }
}

// Utility Functions
function escapeHtml(text) {
    if (text === null || text === undefined) return '';
    const div = document.createElement('div');
    div.textContent = String(text);
    return div.innerHTML;
}

// Pagination Functions
function nextPage(view) {
    const data = view === 'students' ? state.students : 
                 view === 'courses' ? state.courses : state.enrollments;
    const totalPages = Math.ceil(data.length / state.pagination[view].perPage);
    if (state.pagination[view].page < totalPages - 1) {
        state.pagination[view].page++;
        if (view === 'students') renderStudents();
        else if (view === 'courses') renderCourses();
        else if (view === 'enrollments') renderEnrollments();
    }
}

function prevPage(view) {
    if (state.pagination[view].page > 0) {
        state.pagination[view].page--;
        if (view === 'students') renderStudents();
        else if (view === 'courses') renderCourses();
        else if (view === 'enrollments') renderEnrollments();
    }
}

function updatePageInfo(view, data) {
    const { page, perPage } = state.pagination[view];
    const total = data.length;
    const totalPages = Math.ceil(total / perPage);
    const pageInfo = document.getElementById(`${view}-page-info`);
    if (pageInfo) {
        pageInfo.textContent = `第 ${page + 1} / ${totalPages || 1} 页 (共 ${total} 条)`;
    }
}

// Initialize
window.addEventListener('DOMContentLoaded', () => {
    loadStudents();
});
