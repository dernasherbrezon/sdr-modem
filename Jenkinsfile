pipeline {
    agent {
        label 'stretch'
    }
    stages {
        stage('Build') {
            steps {
                sh 'mkdir build'
                sh 'cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build'
                sh 'cmake --build build/ --config Debug'
            }
        }
        stage('Test') {
            steps {
                sh 'cd build && bash ./run_tests.sh'
            }
        }
    }
}